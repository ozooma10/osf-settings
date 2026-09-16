#include "Input/PauseMenu.h"
#include "Menu/OSFSettingsMenu.h"


#include "RE/B/BSFixedString.h"
#include "RE/E/Events.h"
#include "REL/THook.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace OSFSettings
{
	namespace
	{
		constexpr std::ptrdiff_t kQuitCallOffset = 0x48D;
		constexpr std::uint32_t  kActionID = 101;

		using QueueActionHook = REL::THook<void(void*, const RE::BSFixedStringCS*, std::uint32_t, const RE::BSFixedStringCS*, bool)>;
		std::optional<QueueActionHook> g_queueActionHook;
		std::atomic_bool g_registered{};

		void HookedQueueAction(void* a_model, const RE::BSFixedStringCS* a_label, std::uint32_t a_actionType, const RE::BSFixedStringCS* a_confirmText, bool a_disabled)
		{
			(*g_queueActionHook)(a_model, a_label, a_actionType, a_confirmText, a_disabled);
			if (!g_registered.load(std::memory_order_acquire)) {return;}

			// leaked: the engine can touch these strings after static teardown
			static const auto* label = new RE::BSFixedStringCS{ "MOD SETTINGS" };
			static const auto* confirm = new RE::BSFixedStringCS{ "" };
			(*g_queueActionHook)(a_model, label, kActionID, confirm, false);
		}

		class ActionSink final : public RE::BSTEventSink<RE::PauseMenu_StartAction>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::PauseMenu_StartAction& a_event, RE::BSTEventSource<RE::PauseMenu_StartAction>*) override
			{
				if (a_event.GetActionType() != kActionID) {
					return RE::BSEventNotifyControl::kContinue;
				}

				OSFSettingsMenu::Open();
				REX::INFO("PauseMenu: Mod Settings selected");
				return RE::BSEventNotifyControl::kStop;
			}
		};

		ActionSink* Sink()
		{
			static auto* sink = new ActionSink;
			return sink;
		}
	}

	bool PauseMenu::Install()
	{
		if (g_queueActionHook) {
			return g_queueActionHook->GetEnabled();
		}

		g_queueActionHook.emplace("PauseMenu::QueueAction", RE::ID::PauseMenu::RebuildActionList, kQuitCallOffset, &HookedQueueAction);
		if (!g_queueActionHook->Init() || !g_queueActionHook->Enable()) {
			g_queueActionHook.reset();
			return false;
		}

		REX::INFO("PauseMenu: action-list hook installed");
		return true;
	}

	bool PauseMenu::RegisterSink()
	{
		if (g_registered.load(std::memory_order_acquire)) return true;
		if (!g_queueActionHook || !g_queueActionHook->GetEnabled()) return false;

		auto* eventSource = RE::PauseMenu_StartAction::GetEventSource();
		if (!eventSource) {
			REX::ERROR("PauseMenu: PauseMenu_StartAction event source is null; integration disabled");
			return false;
		}

		eventSource->RegisterSink(Sink());
		g_registered.store(true, std::memory_order_release);
		REX::INFO("PauseMenu: native integration armed (action {})", kActionID);
		return true;
	}
}
