#pragma once

#include "../../sdk/OSFPersistenceAPI.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace OSFSettings::Persistence
{
	class PersistenceBroker
	{
	public:
		enum class LogLevel { kDebug, kInfo, kWarning, kError };
		using Logger = std::function<void(LogLevel, std::string_view)>;
		using FormResolver = std::function<bool(std::uint32_t, std::uint32_t&)>;
		using AtomicReplacer = std::function<bool(const std::filesystem::path&, const std::filesystem::path&)>;

		struct SaveStats
		{
			std::uint32_t clientsVisited{};
			std::uint32_t clientsWritten{};
			std::uint32_t clientsFailed{};
			std::uint32_t recordsWritten{};
		};

		struct LoadStats
		{
			std::uint32_t blocksSeen{};
			std::uint32_t clientsLoaded{};
			std::uint32_t unknownClients{};
			std::uint32_t corruptBlocks{};
			std::uint32_t callbackFailures{};
		};

		static constexpr std::uint32_t kFileMagic = OSF_PERSISTENCE_FOURCC('O', 'S', 'F', 'P');
		static constexpr std::uint32_t kFileFormatVersion = 1;
		static constexpr std::uint32_t kMaxClients = 4096;
		static constexpr std::uint32_t kMaxRecordsPerClient = 65536;
		static constexpr std::uint32_t kMaxRecordBytes = 16u * 1024u * 1024u;
		static constexpr std::uint32_t kMaxClientBytes = 64u * 1024u * 1024u;
		static constexpr std::uint64_t kMaxFileBytes = 512ull * 1024ull * 1024ull;

		explicit PersistenceBroker(Logger logger = {});

		bool RegisterClient(const OSFPersistenceClientV1& client);
		bool UnregisterClient(std::uint32_t clientID, void* context);

		std::vector<std::byte> SaveToBytes(std::uint32_t hostVersion, SaveStats* stats = nullptr);
		LoadStats LoadFromBytes(std::span<const std::byte> bytes, const FormResolver& resolver = {},
			bool revertFirst = true);
		void RevertAll();
		void NotifyFormDeleted(std::uint32_t formID);

		bool SaveAtomic(const std::filesystem::path& path, std::uint32_t hostVersion,
			SaveStats* stats = nullptr, const AtomicReplacer& replacer = {});
		LoadStats LoadFile(const std::filesystem::path& path, const FormResolver& resolver = {},
			bool revertFirst = true, bool* found = nullptr);

		static bool DefaultAtomicReplace(const std::filesystem::path& temporary,
			const std::filesystem::path& destination);
		// Internal registration object; public only so the implementation's scoped
		// callback lease can name it without exposing any ABI across DLLs.
		struct Client;

	private:
		using ClientPtr = std::shared_ptr<Client>;

		std::vector<ClientPtr> Snapshot() const;
		void RevertSnapshot(const std::vector<ClientPtr>& clients);
		void Log(LogLevel level, std::string message) const;

		Logger _logger;
		mutable std::mutex _registryMutex;
		std::map<std::uint32_t, ClientPtr> _clients;
		std::mutex _operationMutex;
        std::mutex _fileMutex;
	};
}
