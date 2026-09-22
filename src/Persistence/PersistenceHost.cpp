#include "Persistence/PersistenceHost.h"

#define OSF_PERSISTENCE_HOST 1
#include "../../sdk/OSFPersistenceAPI.h"
#include "Persistence/PersistenceBroker.h"

#include <REL/ASM.h>

#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace OSFSettings::Persistence::PersistenceHost
{
	namespace
	{
		constexpr std::uint32_t kHostVersion = (1u << 16);

		using SaveGameFn = void(RE::BGSSaveLoadGame*, void*, void*, const char*);
		using LoadGameFn = bool(RE::BGSSaveLoadGame*, void*, bool, bool);
		using DeleteSaveFn = bool(RE::BGSSaveLoadManager*, const char*, void*, bool);

		SaveGameFn* g_originalSaveGame{};
		LoadGameFn* g_originalLoadGame{};
		DeleteSaveFn* g_originalDeleteSave{};

		constexpr REL::ID kDeleteSave{ 98620 };
		constexpr std::array<std::uint8_t, 15> kSavePrologue{
			0x4C, 0x89, 0x4C, 0x24, 0x20,
			0x4C, 0x89, 0x44, 0x24, 0x18,
			0x48, 0x89, 0x54, 0x24, 0x10
		};
		constexpr std::array<std::uint8_t, 15> kLoadPrologue{
			0x48, 0x8B, 0xC4,
			0x44, 0x88, 0x48, 0x20,
			0x44, 0x88, 0x40, 0x18,
			0x48, 0x89, 0x50, 0x10
		};
		constexpr std::array<std::uint8_t, 15> kDeletePrologue{
			0x48, 0x89, 0x5C, 0x24, 0x08,
			0x48, 0x89, 0x74, 0x24, 0x10,
			0x48, 0x89, 0x7C, 0x24, 0x20
		};

		// IDs 98376, 98380, and 98620 resolve the save, load, and delete workers on
		// both 1.16.242 and 1.16.244. Prologue gates below still reject signature drift.

        template <std::size_t N>
        bool PrologueMatches(REL::ID id, const std::array<std::uint8_t, N>& expected)
        {
            return id.id() && std::memcmp(reinterpret_cast<const void*>(id.address()), expected.data(), N) == 0;
        }

		std::string BoundedName(const char* name)
		{
			if (!name) {
				return {};
			}
			const std::size_t length = ::strnlen(name, 512);
			return length < 512 ? std::string(name, length) : std::string{};
		}

		std::filesystem::path SaveDirectory()
		{
			try {
				if (auto logs = SFSE::log::log_directory()) {
					std::filesystem::path root = logs->parent_path().parent_path();
					if (auto* ini = RE::INISettingCollection::GetSingleton()) {
						if (auto* setting = ini->GetSetting("sLocalSavePath:General");
							setting && setting->GetType() == RE::Setting::Type::kString) {
							const auto local = setting->GetString();
							if (!local.empty()) {
								return root / std::filesystem::path(std::string(local));
							}
						}
					}
					return root / "Saves";
				}
			} catch (...) {
			}
			return {};
		}

		std::filesystem::path SidecarPath(std::string_view saveName)
		{
			if (saveName.empty()) {
				return {};
			}
			std::filesystem::path name{ std::string(saveName) };
			if (name.has_parent_path() || name.filename() != name) {
				REX::ERROR("[Save] rejected unsafe save name '{}'", saveName);
				return {};
			}
			name.replace_extension(".osf");
			const auto directory = SaveDirectory();
			return directory.empty() ? std::filesystem::path{} : directory / name;
		}

		std::string ClientIDText(std::uint32_t id)
		{
			char text[5]{};
			for (std::size_t i = 0; i < 4; ++i) {
				const auto ch = static_cast<unsigned char>((id >> (i * 8)) & 0xFF);
				text[i] = ch >= 0x20 && ch <= 0x7E ? static_cast<char>(ch) : '?';
			}
			return text;
		}

		void BrokerLog(PersistenceBroker::LogLevel level, std::string_view message)
		{
			switch (level) {
			case PersistenceBroker::LogLevel::kDebug: REX::DEBUG("[Save] {}", message); break;
			case PersistenceBroker::LogLevel::kInfo: REX::INFO("[Save] {}", message); break;
			case PersistenceBroker::LogLevel::kWarning: REX::WARN("[Save] {}", message); break;
			case PersistenceBroker::LogLevel::kError: REX::ERROR("[Save] {}", message); break;
			}
		}

		class Host
		{
		public:
			static Host& GetSingleton()
			{
				static auto* singleton = new Host;
				return *singleton;
			}

			bool Initialize()
			{
				bool expected = false;
				if (!_initialized.compare_exchange_strong(expected, true)) {
					return _ready.load(std::memory_order_acquire);
				}

				const bool hooks = InstallHooks();
				_ready.store(hooks, std::memory_order_release);
				if (hooks) {
					REX::INFO("[Save] persistence host initialized (ABI {:#x}, format {}, magic OSFP)",
						OSF_PERSISTENCE_API_VERSION, PersistenceBroker::kFileFormatVersion);
				} else {
					REX::ERROR("[Save] persistence host unavailable: one or more save lifecycle hooks failed validation");
				}
				return hooks;
			}

			bool IsReady() const { return _ready.load(std::memory_order_acquire); }

			bool Register(const OSFPersistenceClientV1* descriptor)
			{
				if (!IsReady() || !descriptor) {
					return false;
				}
				if (!_broker.RegisterClient(*descriptor)) {
					return false;
				}
				REX::INFO("[API] persistence client registered: {} ('{}')", ClientIDText(descriptor->clientID), descriptor->clientName);
				return true;
			}

			bool Unregister(std::uint32_t id, void* context)
			{
				if (!_broker.UnregisterClient(id, context)) {
					return false;
				}
				REX::INFO("[API] persistence client unregistered: {}", ClientIDText(id));
				return true;
			}

			void BeginLoad(const char* reason)
			{
				std::lock_guard lifecycleLock(_lifecycleMutex);
				if (_loadInProgress.load(std::memory_order_acquire)) {
					return;
				}
				_loadInProgress.store(true, std::memory_order_release);
				_loadDispatched = false;
				_completedAwaitingBackstop = false;
				{
					std::lock_guard remapLock(_remapMutex);
					_remapped.clear();
					_deleted.clear();
				}
				REX::DEBUG("[Save] persistence revert before load ({})", reason ? reason : "unknown");
				_broker.RevertAll();
			}

			// The load worker actually ran: from here on only FinishLoad/Backstop may close the window.
			void MarkLoadDispatched()
			{
				std::lock_guard lifecycleLock(_lifecycleMutex);
				_loadDispatched = true;
			}

			// A load op ended in a terminal status without ever reaching the load worker (refused /
			// cancelled before dispatch). Un-latch so the next load gets its full revert window; a
			// dispatched load is left alone (FinishLoad owns it).
			void AbortLoad()
			{
				std::lock_guard lifecycleLock(_lifecycleMutex);
				if (!_loadInProgress.load(std::memory_order_acquire) || _loadDispatched) {
					return;
				}
				REX::DEBUG("[Save] load refused before dispatch — closing the revert window");
				_loadInProgress.store(false, std::memory_order_release);
				std::lock_guard remapLock(_remapMutex);
				_remapped.clear();
				_deleted.clear();
			}

			void FinishLoad(std::string_view saveName, bool succeeded)
			{
				BeginLoad("load-name hook fallback");
				PersistenceBroker::LoadStats stats;
				bool found = false;
				const auto path = SidecarPath(saveName);
				if (succeeded && !path.empty()) {
					stats = _broker.LoadFile(path,
						[this](std::uint32_t oldID, std::uint32_t& resolved) { return ResolveFormID(oldID, resolved); },
						false, &found);
				}
				REX::INFO("[Save] sidecar load '{}': {} (blocks={}, loaded={}, unknown={}, corrupt={}, callbackFailures={})",
					path.empty() ? std::string(saveName) : path.filename().string(),
					!succeeded ? "game load failed; skipped" : (found ? "parsed" : "not found (clean load)"),
					stats.blocksSeen, stats.clientsLoaded, stats.unknownClients, stats.corruptBlocks, stats.callbackFailures);
				std::lock_guard lifecycleLock(_lifecycleMutex);
				_loadInProgress.store(false, std::memory_order_release);
				_loadDispatched = false;
				_completedAwaitingBackstop = succeeded;
				std::lock_guard remapLock(_remapMutex);
				_remapped.clear();
				_deleted.clear();
			}

			void Backstop()
			{
				{
					std::lock_guard lifecycleLock(_lifecycleMutex);
					if (_completedAwaitingBackstop) {
						_completedAwaitingBackstop = false;
						return;
					}
				}
				BeginLoad("TESLoadGameEvent backstop (unkeyed load/new game)");
				std::lock_guard lifecycleLock(_lifecycleMutex);
				_loadInProgress.store(false, std::memory_order_release);
				_loadDispatched = false;
				std::lock_guard remapLock(_remapMutex);
				_remapped.clear();
				_deleted.clear();
			}

			void OnRemap(std::uint32_t oldID, std::uint32_t newID)
			{
				std::lock_guard lock(_remapMutex);
				if (_loadInProgress.load(std::memory_order_acquire)) {
					_remapped[oldID] = newID;
				}
			}

			void OnDeleted(std::uint32_t formID)
			{
				{
					std::lock_guard lock(_remapMutex);
					if (_loadInProgress.load(std::memory_order_acquire)) {
						_deleted.insert(formID);
					}
				}
				_broker.NotifyFormDeleted(formID);
			}

			bool ResolveFormID(std::uint32_t oldID, std::uint32_t& resolved)
			{
				std::lock_guard lock(_remapMutex);
				if (const auto it = _remapped.find(oldID); it != _remapped.end()) {
					resolved = it->second;
					return resolved != 0;
				}
				if (_deleted.contains(oldID)) {
					return false;
				}
				resolved = oldID;
				return true;
			}

			void Save(std::string_view saveName)
			{
				const auto path = SidecarPath(saveName);
				if (path.empty()) {
					REX::ERROR("[Save] sidecar save skipped: save path unavailable");
					return;
				}
				PersistenceBroker::SaveStats stats;
				const bool ok = _broker.SaveAtomic(path, kHostVersion, &stats);
				REX::INFO("[Save] sidecar save '{}': {} (clients={}/{}, records={}, callbackFailures={})",
					path.filename().string(), ok ? "complete" : "failed (previous preserved)",
					stats.clientsWritten, stats.clientsVisited, stats.recordsWritten, stats.clientsFailed);
			}

			void Delete(std::string_view saveName)
			{
				const auto path = SidecarPath(saveName);
				if (path.empty()) {
					return;
				}
				std::error_code ec;
				const bool removed = std::filesystem::remove(path, ec);
				if (ec) {
					REX::WARN("[Save] could not delete sidecar '{}' ({})", path.filename().string(), ec.message());
				} else if (removed) {
					REX::DEBUG("[Save] deleted sidecar '{}'", path.filename().string());
				}
			}

			PersistenceBroker& Broker() { return _broker; }

		private:
			Host() : _broker(BrokerLog) {}

			template <class Fn>
			static Fn* MakeGateway(std::uintptr_t target, std::size_t stolenBytes)
			{
				auto& trampoline = REL::GetTrampoline();
				auto* code = static_cast<std::byte*>(trampoline.allocate(stolenBytes + sizeof(REL::ASM::JMP14)));
				std::memcpy(code, reinterpret_cast<const void*>(target), stolenBytes);
				REL::ASM::JMP14 jump(target + stolenBytes);
				std::memcpy(code + stolenBytes, &jump, sizeof(jump));
				return reinterpret_cast<Fn*>(code);
			}

			bool InstallHooks()
			{
				const auto saveTarget = RE::ID::BGSSaveLoadGame::SaveGame.address();
				const auto loadTarget = RE::ID::BGSSaveLoadGame::LoadGame.address();
				const auto deleteTarget = kDeleteSave.address();
				const bool saveOK = PrologueMatches(RE::ID::BGSSaveLoadGame::SaveGame, kSavePrologue);
				const bool loadOK = PrologueMatches(RE::ID::BGSSaveLoadGame::LoadGame, kLoadPrologue);
				const bool deleteOK = PrologueMatches(kDeleteSave, kDeletePrologue);
				if (!saveOK || !loadOK || !deleteOK) {
					REX::ERROR("[Save] persistence hook validation failed: save={} load={} delete={}", saveOK, loadOK, deleteOK);
					return false;
				}
				auto& trampoline = REL::GetTrampoline();
				g_originalSaveGame = MakeGateway<SaveGameFn>(saveTarget, 5);
				g_originalLoadGame = MakeGateway<LoadGameFn>(loadTarget, 7);
				g_originalDeleteSave = MakeGateway<DeleteSaveFn>(deleteTarget, 5);
				trampoline.write_jmp<5>(saveTarget, &SaveHook);
				trampoline.write_jmp<5>(loadTarget, &LoadHook);
				trampoline.write_jmp<5>(deleteTarget, &DeleteHook);
				REX::DEBUG("[Save] installed Address Library save/load/delete entry hooks");
				return g_originalSaveGame && g_originalLoadGame && g_originalDeleteSave;
			}

			static void SaveHook(RE::BGSSaveLoadGame* self, void* context, void* writer, const char* name)
			{
				const std::string saveName = BoundedName(name);
				g_originalSaveGame(self, context, writer, name);
				if (!saveName.empty()) {
					GetSingleton().Save(saveName);
				}
			}

			static bool LoadHook(RE::BGSSaveLoadGame* self, void* reader, bool flag1, bool flag2)
			{
				const std::string saveName = BoundedName(static_cast<const char*>(reader));
				auto& host = GetSingleton();
				host.BeginLoad("load-name hook");
				host.MarkLoadDispatched();
				const bool result = g_originalLoadGame(self, reader, flag1, flag2);
				host.FinishLoad(saveName, result);
				return result;
			}

			static bool DeleteHook(RE::BGSSaveLoadManager* self, const char* name, void* unknown, bool flag)
			{
				const std::string saveName = BoundedName(name);
				const bool result = g_originalDeleteSave(self, name, unknown, flag);
				if (result && !saveName.empty()) {
					GetSingleton().Delete(saveName);
				}
				return result;
			}

			PersistenceBroker _broker;
			std::atomic<bool> _initialized{};
			std::atomic<bool> _ready{};
			std::mutex _lifecycleMutex;
			std::atomic<bool> _loadInProgress{};
			bool _loadDispatched{};  // guarded by _lifecycleMutex
			bool _completedAwaitingBackstop{};
			std::mutex _remapMutex;
			std::unordered_map<std::uint32_t, std::uint32_t> _remapped;
			std::unordered_set<std::uint32_t> _deleted;
		};

		class PersistenceEvents final :
			public RE::BSTEventSink<RE::TESFormIDRemapEvent>,
			public RE::BSTEventSink<RE::TESFormDeleteEvent>,
            public RE::BSTEventSink<RE::TESLoadGameEvent>,
            public RE::BSTEventSink<RE::SaveLoadEvent>
		{
		public:
			static PersistenceEvents* GetSingleton()
			{
				static auto* sink = new PersistenceEvents;
				return sink;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESFormIDRemapEvent& event,
				RE::BSTEventSource<RE::TESFormIDRemapEvent>*) override
			{
				Host::GetSingleton().OnRemap(event.oldFormID, event.newFormID);
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent& event,
				RE::BSTEventSource<RE::TESFormDeleteEvent>*) override
			{
				Host::GetSingleton().OnDeleted(event.formID);
				return RE::BSEventNotifyControl::kContinue;
			}
            RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent&,
                RE::BSTEventSource<RE::TESLoadGameEvent>*) override
            {
                Host::GetSingleton().Backstop();
                return RE::BSEventNotifyControl::kContinue;
            }

            RE::BSEventNotifyControl ProcessEvent(const RE::SaveLoadEvent& event,
                RE::BSTEventSource<RE::SaveLoadEvent>*) override
            {
                using Op = RE::SaveLoadEvent::OpType;
                const bool replacesWorld = event.opType == Op::kLoadMostRecent || event.opType == Op::kQuickload ||
                    event.opType == Op::kLoad || event.opType == Op::kLoadNamedFile;
                if (replacesWorld) {
                    if (event.status == RE::SaveLoadEvent::Status::kBegin) Host::GetSingleton().BeginLoad("SaveLoadEvent begin");
                    else Host::GetSingleton().AbortLoad();
                }
                return RE::BSEventNotifyControl::kContinue;
            }

		};

		std::uint8_t OSF_PERSISTENCE_CALL APIIsReady(const OSFPersistenceAPI* api)
		{
			return api && api->context && static_cast<Host*>(api->context)->IsReady();
		}

		std::uint8_t OSF_PERSISTENCE_CALL APIRegister(const OSFPersistenceAPI* api, const OSFPersistenceClientV1* client)
		{
			return api && api->context && static_cast<Host*>(api->context)->Register(client);
		}

		std::uint8_t OSF_PERSISTENCE_CALL APIUnregister(const OSFPersistenceAPI* api, std::uint32_t id, void* context)
		{
			return api && api->context && static_cast<Host*>(api->context)->Unregister(id, context);
		}

		std::uint32_t OSF_PERSISTENCE_CALL APIVersion(const OSFPersistenceAPI*)
		{
			return OSF_PERSISTENCE_API_VERSION;
		}

		void OSF_PERSISTENCE_CALL APIPluginVersion(const OSFPersistenceAPI*,
			std::uint32_t* major, std::uint32_t* minor, std::uint32_t* patch)
		{
			const auto version = SFSE::GetPluginVersion();
			if (major) *major = version.major();
			if (minor) *minor = version.minor();
			if (patch) *patch = version.patch();
		}

		const OSFPersistenceAPI& APITable()
		{
			static const OSFPersistenceAPI api{
				sizeof(OSFPersistenceAPI), &Host::GetSingleton(),
				&APIIsReady, &APIRegister, &APIUnregister, &APIVersion, &APIPluginVersion
			};
			return api;
		}
	}

	bool Initialize() { return Host::GetSingleton().Initialize(); }

    void RegisterEventSinks()
    {
        static bool remap{}, deleted{}, loaded{}, lifecycle{};
        if (remap && deleted && loaded && lifecycle) return;
        auto* sink = PersistenceEvents::GetSingleton();
        if (!remap) {
            if (auto* source = RE::TESFormIDRemapEvent::GetEventSource()) {
                source->RegisterSink(static_cast<RE::BSTEventSink<RE::TESFormIDRemapEvent>*>(sink));
                remap = true;
            }
        }
        if (!deleted) {
            if (auto* source = RE::TESFormDeleteEvent::GetEventSource()) {
                source->RegisterSink(static_cast<RE::BSTEventSink<RE::TESFormDeleteEvent>*>(sink));
                deleted = true;
            }
        }
        if (!loaded) {
            if (auto* source = RE::TESLoadGameEvent::GetEventSource()) {
                source->RegisterSink(static_cast<RE::BSTEventSink<RE::TESLoadGameEvent>*>(sink));
                loaded = true;
            }
        }
        constexpr std::array<std::uint8_t, 13> eventPrologue{
            0x48, 0x83, 0xEC, 0x28, 0x65, 0x48, 0x8B, 0x04, 0x25, 0x58, 0x00, 0x00, 0x00
        };
        if (!lifecycle && PrologueMatches(RE::ID::SaveLoadEvent::GetEventSource, eventPrologue)) {
            if (auto* source = RE::SaveLoadEvent::GetEventSource()) {
                source->RegisterSink(static_cast<RE::BSTEventSink<RE::SaveLoadEvent>*>(sink));
                lifecycle = true;
            }
        }
        if (!remap || !deleted || !loaded || !lifecycle) {
            REX::WARN("[Save] persistence event sinks incomplete: remap={} delete={} loaded={} lifecycle={}",
                remap, deleted, loaded, lifecycle);
        }
    }


	const OSFPersistenceAPI* RequestAPI(std::uint32_t requestedVersion)
	{
		if (OSF_PERSISTENCE_API_MAJOR(requestedVersion) != OSF_PERSISTENCE_API_MAJOR(OSF_PERSISTENCE_API_VERSION) ||
			OSF_PERSISTENCE_API_MINOR(requestedVersion) > OSF_PERSISTENCE_API_MINOR(OSF_PERSISTENCE_API_VERSION)) {
			return nullptr;
		}
		return &APITable();
	}
}

OSF_PERSISTENCE_EXPORT const OSFPersistenceAPI* OSF_PERSISTENCE_CALL
	OSF_RequestPersistenceAPI(std::uint32_t requestedVersion)
{
	return OSFSettings::Persistence::PersistenceHost::RequestAPI(requestedVersion);
}
