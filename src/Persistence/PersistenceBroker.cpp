#include "Persistence/PersistenceBroker.h"
#include "Persistence/AtomicFile.h"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <io.h>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace OSFSettings::Persistence
{
	namespace
	{
		constexpr std::uint32_t kFileHeaderSize = 24;
		constexpr std::uint32_t kClientHeaderSize = 20;
		constexpr std::uint32_t kRecordHeaderSize = 12;

		thread_local const void* g_activeClient = nullptr;

		std::string ClientIDText(std::uint32_t id)
		{
			char text[5]{};
			for (std::size_t i = 0; i < 4; ++i) {
				const auto ch = static_cast<unsigned char>((id >> (i * 8)) & 0xFF);
				text[i] = ch >= 0x20 && ch <= 0x7E ? static_cast<char>(ch) : '?';
			}
			char result[32]{};
			std::snprintf(result, sizeof(result), "'%s' (0x%08X)", text, id);
			return result;
		}

		void AppendU32(std::vector<std::byte>& out, std::uint32_t value)
		{
			out.push_back(static_cast<std::byte>(value & 0xFF));
			out.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
			out.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
			out.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
		}

		void PatchU32(std::vector<std::byte>& out, std::size_t offset, std::uint32_t value)
		{
			for (std::size_t i = 0; i < 4; ++i) {
				out[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFF);
			}
		}

		bool ReadU32(std::span<const std::byte> bytes, std::size_t& offset, std::uint32_t& value)
		{
			if (offset > bytes.size() || bytes.size() - offset < 4) {
				return false;
			}
			value = static_cast<std::uint32_t>(bytes[offset]) |
				(static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
				(static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
				(static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
			offset += 4;
			return true;
		}

		std::uint32_t CRC32(std::span<const std::byte> bytes)
		{
			std::uint32_t crc = 0xFFFFFFFFu;
			for (const std::byte b : bytes) {
				crc ^= static_cast<std::uint8_t>(b);
				for (int bit = 0; bit < 8; ++bit) {
					crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
				}
			}
			return ~crc;
		}

		struct Record
		{
			std::uint32_t type{};
			std::uint32_t version{};
			std::vector<std::byte> data;
		};

		struct WriterState
		{
			std::vector<Record> records;
			std::uint64_t totalBytes{};
			bool failed{};

			static std::uint8_t OSF_PERSISTENCE_CALL Open(
				OSFRecordWriterV1* writer, std::uint32_t type, std::uint32_t version)
			{
				if (!writer || !writer->context) {
					return 0;
				}
				auto& self = *static_cast<WriterState*>(writer->context);
				if (self.failed || self.records.size() >= PersistenceBroker::kMaxRecordsPerClient) {
					self.failed = true;
					return 0;
				}
				self.records.push_back({ type, version, {} });
				return 1;
			}

			static std::uint8_t OSF_PERSISTENCE_CALL Write(
				OSFRecordWriterV1* writer, const void* data, std::uint32_t length)
			{
				if (!writer || !writer->context) {
					return 0;
				}
				auto& self = *static_cast<WriterState*>(writer->context);
				if (self.failed || self.records.empty() || (length && !data) ||
					self.records.back().data.size() + length > PersistenceBroker::kMaxRecordBytes ||
					self.totalBytes + length + kRecordHeaderSize > PersistenceBroker::kMaxClientBytes) {
					self.failed = true;
					return 0;
				}
				const auto* first = static_cast<const std::byte*>(data);
				if (length) {
					self.records.back().data.insert(self.records.back().data.end(), first, first + length);
				}
				self.totalBytes += length;
				return 1;
			}
		};

		struct ParsedRecord
		{
			std::uint32_t type{};
			std::uint32_t version{};
			std::span<const std::byte> data;
		};

		struct ReaderState
		{
			std::vector<ParsedRecord> records;
			std::size_t nextRecord{};
			const ParsedRecord* current{};
			std::size_t currentOffset{};
			const PersistenceBroker::FormResolver* resolver{};

			static std::uint8_t OSF_PERSISTENCE_CALL Next(OSFRecordReaderV1* reader,
				std::uint32_t* outType, std::uint32_t* outVersion, std::uint32_t* outLength)
			{
				if (!reader || !reader->context || !outType || !outVersion || !outLength) {
					return 0;
				}
				auto& self = *static_cast<ReaderState*>(reader->context);
				self.current = nullptr;  // unread tail is discarded by changing the bounded span
				self.currentOffset = 0;
				if (self.nextRecord >= self.records.size()) {
					return 0;
				}
				self.current = &self.records[self.nextRecord++];
				*outType = self.current->type;
				*outVersion = self.current->version;
				*outLength = static_cast<std::uint32_t>(self.current->data.size());
				return 1;
			}

			static std::uint32_t OSF_PERSISTENCE_CALL Read(
				OSFRecordReaderV1* reader, void* data, std::uint32_t length)
			{
				if (!reader || !reader->context || (length && !data)) {
					return 0;
				}
				auto& self = *static_cast<ReaderState*>(reader->context);
				if (!self.current || self.currentOffset > self.current->data.size()) {
					return 0;
				}
				const std::size_t available = self.current->data.size() - self.currentOffset;
				const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(length, available));
				if (count) {
					std::memcpy(data, self.current->data.data() + self.currentOffset, count);
					self.currentOffset += count;
				}
				return count;
			}

			static std::uint8_t OSF_PERSISTENCE_CALL Resolve(OSFRecordReaderV1* reader,
				std::uint32_t oldID, std::uint32_t* outID)
			{
				if (!reader || !reader->context || !outID) {
					return 0;
				}
				auto& self = *static_cast<ReaderState*>(reader->context);
				if (!self.resolver || !*self.resolver) {
					*outID = oldID;
					return 1;
				}
				std::uint32_t resolved{};
				if (!(*self.resolver)(oldID, resolved)) {
					return 0;
				}
				*outID = resolved;
				return 1;
			}
		};
	}

	struct PersistenceBroker::Client
	{
		std::uint32_t id{};
		std::string name;
		void* context{};
		decltype(OSFPersistenceClientV1::save) save{};
		decltype(OSFPersistenceClientV1::load) load{};
		decltype(OSFPersistenceClientV1::revert) revert{};
		decltype(OSFPersistenceClientV1::formDeleted) formDeleted{};

		std::mutex lifetimeMutex;
		std::condition_variable lifetimeCV;
		bool active{ true };
		std::uint32_t inFlight{};
	};

	namespace
	{
		class ClientLease
		{
		public:
			explicit ClientLease(const std::shared_ptr<PersistenceBroker::Client>& client) : _client(client)
			{
				std::lock_guard lock(_client->lifetimeMutex);
				if (_client->active) {
					++_client->inFlight;
					_acquired = true;
				}
			}

			~ClientLease()
			{
				if (!_acquired) {
					return;
				}
				std::lock_guard lock(_client->lifetimeMutex);
				if (--_client->inFlight == 0) {
					_client->lifetimeCV.notify_all();
				}
			}

			explicit operator bool() const { return _acquired; }

		private:
			std::shared_ptr<PersistenceBroker::Client> _client;
			bool _acquired{};
		};
	}

	PersistenceBroker::PersistenceBroker(Logger logger) : _logger(std::move(logger)) {}

	void PersistenceBroker::Log(LogLevel level, std::string message) const
	{
		if (_logger) {
			_logger(level, message);
		}
	}

	bool PersistenceBroker::RegisterClient(const OSFPersistenceClientV1& descriptor)
	{
		if (descriptor.size < sizeof(OSFPersistenceClientV1) || descriptor.clientID == 0 || !descriptor.clientName) {
			Log(LogLevel::kError, "rejected invalid persistence client descriptor");
			return false;
		}
		std::string name(descriptor.clientName, ::strnlen(descriptor.clientName, 256));
		if (name.empty() || name.size() == 256) {
			Log(LogLevel::kError, "rejected client " + ClientIDText(descriptor.clientID) + ": invalid name");
			return false;
		}

		auto client = std::make_shared<Client>();
		client->id = descriptor.clientID;
		client->name = std::move(name);
		client->context = descriptor.context;
		client->save = descriptor.save;
		client->load = descriptor.load;
		client->revert = descriptor.revert;
		client->formDeleted = descriptor.formDeleted;

		std::lock_guard lock(_registryMutex);
		if (const auto it = _clients.find(client->id); it != _clients.end()) {
			Log(LogLevel::kError, "client ID collision/duplicate for " + ClientIDText(client->id) +
				": existing='" + it->second->name + "', rejected='" + client->name + "'");
			return false;
		}
		_clients.emplace(client->id, std::move(client));
		return true;
	}

	bool PersistenceBroker::UnregisterClient(std::uint32_t clientID, void* context)
	{
		ClientPtr client;
		{
			std::lock_guard lock(_registryMutex);
			const auto it = _clients.find(clientID);
			if (it == _clients.end() || it->second->context != context) {
				return false;
			}
			client = it->second;
			_clients.erase(it);
		}
		std::unique_lock lock(client->lifetimeMutex);
		client->active = false;
		if (g_activeClient != client.get()) {
			client->lifetimeCV.wait(lock, [&] { return client->inFlight == 0; });
		}
		return true;
	}

	std::vector<PersistenceBroker::ClientPtr> PersistenceBroker::Snapshot() const
	{
		std::lock_guard lock(_registryMutex);
		std::vector<ClientPtr> result;
		result.reserve(_clients.size());
		for (const auto& [id, client] : _clients) {
			(void)id;
			result.push_back(client);
		}
		return result;
	}

	std::vector<std::byte> PersistenceBroker::SaveToBytes(std::uint32_t hostVersion, SaveStats* outStats)
	{
		std::lock_guard operationLock(_operationMutex);
		SaveStats stats;
		std::vector<std::byte> output;
		output.reserve(kFileHeaderSize);
		AppendU32(output, kFileMagic);
		AppendU32(output, kFileFormatVersion);
		AppendU32(output, kFileHeaderSize);
		AppendU32(output, hostVersion);
		AppendU32(output, 0);  // client count patched below
		AppendU32(output, 0);  // flags/reserved

		for (const auto& client : Snapshot()) {
			++stats.clientsVisited;
			if (!client->save) {
				continue;
			}
			ClientLease lease(client);
			if (!lease) {
				continue;
			}
			WriterState state;
			OSFRecordWriterV1 writer{ sizeof(writer), &state, &WriterState::Open, &WriterState::Write };
			bool ok = false;
			const void* previous = g_activeClient;
			g_activeClient = client.get();
			try {
				ok = client->save(client->context, &writer) != 0;
			} catch (...) {
				Log(LogLevel::kError, "save callback threw for " + ClientIDText(client->id) + " ('" + client->name + "')");
			}
			g_activeClient = previous;
			if (!ok || state.failed) {
				++stats.clientsFailed;
				Log(LogLevel::kError, "omitted failed save block for " + ClientIDText(client->id) + " ('" + client->name + "')");
				continue;
			}
			if (state.records.empty()) {
				continue;
			}

			std::vector<std::byte> payload;
			for (const auto& record : state.records) {
				AppendU32(payload, record.type);
				AppendU32(payload, record.version);
				AppendU32(payload, static_cast<std::uint32_t>(record.data.size()));
				payload.insert(payload.end(), record.data.begin(), record.data.end());
			}
			if (payload.size() > kMaxClientBytes || output.size() + kClientHeaderSize + payload.size() > kMaxFileBytes) {
				++stats.clientsFailed;
				Log(LogLevel::kError, "omitted oversized save block for " + ClientIDText(client->id));
				continue;
			}

			AppendU32(output, client->id);
			AppendU32(output, static_cast<std::uint32_t>(state.records.size()));
			AppendU32(output, static_cast<std::uint32_t>(payload.size()));
			AppendU32(output, CRC32(payload));
			AppendU32(output, 0);
			output.insert(output.end(), payload.begin(), payload.end());
			++stats.clientsWritten;
			stats.recordsWritten += static_cast<std::uint32_t>(state.records.size());
		}
		PatchU32(output, 16, stats.clientsWritten);
		if (outStats) {
			*outStats = stats;
		}
		return output;
	}

	void PersistenceBroker::RevertSnapshot(const std::vector<ClientPtr>& clients)
	{
		for (const auto& client : clients) {
			if (!client->revert) {
				continue;
			}
			ClientLease lease(client);
			if (!lease) {
				continue;
			}
			const void* previous = g_activeClient;
			g_activeClient = client.get();
			try {
				client->revert(client->context);
			} catch (...) {
				Log(LogLevel::kError, "revert callback threw for " + ClientIDText(client->id) + " ('" + client->name + "')");
			}
			g_activeClient = previous;
		}
	}

	void PersistenceBroker::RevertAll()
	{
		std::lock_guard operationLock(_operationMutex);
		RevertSnapshot(Snapshot());
	}

	PersistenceBroker::LoadStats PersistenceBroker::LoadFromBytes(
		std::span<const std::byte> bytes, const FormResolver& resolver, bool revertFirst)
	{
		std::lock_guard operationLock(_operationMutex);
		LoadStats stats;
		const auto clients = Snapshot();
		if (revertFirst) {
			RevertSnapshot(clients);
		}
		if (bytes.empty()) {
			return stats;
		}
		if (bytes.size() > kMaxFileBytes) {
			Log(LogLevel::kError, "sidecar exceeds the file-size cap");
			return stats;
		}

		std::size_t offset = 0;
		std::uint32_t magic{}, format{}, headerSize{}, hostVersion{}, clientCount{}, flags{};
		if (!ReadU32(bytes, offset, magic) || !ReadU32(bytes, offset, format) ||
			!ReadU32(bytes, offset, headerSize) || !ReadU32(bytes, offset, hostVersion) ||
			!ReadU32(bytes, offset, clientCount) || !ReadU32(bytes, offset, flags)) {
			Log(LogLevel::kError, "truncated persistence file header");
			return stats;
		}
		(void)hostVersion;
		(void)flags;
		if (magic != kFileMagic || format != kFileFormatVersion || headerSize < kFileHeaderSize ||
			headerSize > bytes.size() || clientCount > kMaxClients) {
			Log(LogLevel::kError, "invalid persistence file header/version/count");
			return stats;
		}
		offset = headerSize;

		std::unordered_map<std::uint32_t, ClientPtr> byID;
		for (const auto& client : clients) {
			byID.emplace(client->id, client);
		}
		std::unordered_set<std::uint32_t> dispatched;

		for (std::uint32_t blockIndex = 0; blockIndex < clientCount; ++blockIndex) {
			++stats.blocksSeen;
			std::uint32_t clientID{}, recordCount{}, payloadLength{}, checksum{}, reserved{};
			if (!ReadU32(bytes, offset, clientID) || !ReadU32(bytes, offset, recordCount) ||
				!ReadU32(bytes, offset, payloadLength) || !ReadU32(bytes, offset, checksum) ||
				!ReadU32(bytes, offset, reserved)) {
				Log(LogLevel::kError, "truncated client header at block " + std::to_string(blockIndex));
				break;
			}
			(void)reserved;
			if (offset > bytes.size() || payloadLength > bytes.size() - offset) {
				Log(LogLevel::kError, "truncated payload for client " + ClientIDText(clientID));
				break;  // no trustworthy boundary for a following block
			}
			const auto payload = bytes.subspan(offset, payloadLength);
			offset += payloadLength;

			if (recordCount > kMaxRecordsPerClient || payloadLength > kMaxClientBytes || CRC32(payload) != checksum) {
				++stats.corruptBlocks;
				Log(LogLevel::kWarning, "skipped corrupt/oversized block for " + ClientIDText(clientID));
				continue;
			}

			ReaderState state;
			state.resolver = &resolver;
			std::size_t recordOffset = 0;
			bool valid = true;
			state.records.reserve(recordCount);
			for (std::uint32_t i = 0; i < recordCount; ++i) {
				std::uint32_t type{}, version{}, length{};
				if (!ReadU32(payload, recordOffset, type) || !ReadU32(payload, recordOffset, version) ||
					!ReadU32(payload, recordOffset, length) || length > kMaxRecordBytes ||
					recordOffset > payload.size() || length > payload.size() - recordOffset) {
					valid = false;
					break;
				}
				state.records.push_back({ type, version, payload.subspan(recordOffset, length) });
				recordOffset += length;
			}
			if (!valid || recordOffset != payload.size()) {
				++stats.corruptBlocks;
				Log(LogLevel::kWarning, "skipped malformed record stream for " + ClientIDText(clientID));
				continue;
			}

			const auto found = byID.find(clientID);
			if (found == byID.end()) {
				++stats.unknownClients;
				// Name the orphan: this block will NOT be carried into the next save (the host cannot
				// remap FormIDs inside an opaque payload), so the plugin's data is gone if it stays
				// uninstalled — an aggregate count alone hides which mod lost state.
				Log(LogLevel::kWarning, "no registered client for saved block " + ClientIDText(clientID) +
					" — its data will not be carried into the next save (plugin uninstalled or not yet registered)");
				continue;
			}
			if (!dispatched.insert(clientID).second) {
				++stats.corruptBlocks;
				Log(LogLevel::kWarning, "skipped duplicate client block for " + ClientIDText(clientID));
				continue;
			}
			const auto& client = found->second;
			if (!client->load) {
				continue;
			}
			ClientLease lease(client);
			if (!lease) {
				continue;
			}
			OSFRecordReaderV1 reader{ sizeof(reader), &state, &ReaderState::Next, &ReaderState::Read, &ReaderState::Resolve };
			bool ok = false;
			const void* previous = g_activeClient;
			g_activeClient = client.get();
			try {
				ok = client->load(client->context, &reader) != 0;
			} catch (...) {
				Log(LogLevel::kError, "load callback threw for " + ClientIDText(clientID) + " ('" + client->name + "')");
			}
			g_activeClient = previous;
			if (ok) {
				++stats.clientsLoaded;
			} else {
				++stats.callbackFailures;
				Log(LogLevel::kError, "load callback failed for " + ClientIDText(clientID) + " ('" + client->name + "')");
			}
		}
		return stats;
	}

	void PersistenceBroker::NotifyFormDeleted(std::uint32_t formID)
	{
		std::lock_guard operationLock(_operationMutex);
		for (const auto& client : Snapshot()) {
			if (!client->formDeleted) {
				continue;
			}
			ClientLease lease(client);
			if (!lease) {
				continue;
			}
			const void* previous = g_activeClient;
			g_activeClient = client.get();
			try {
				client->formDeleted(client->context, formID);
			} catch (...) {
				Log(LogLevel::kError, "formDeleted callback threw for " + ClientIDText(client->id) + " ('" + client->name + "')");
			}
			g_activeClient = previous;
		}
	}

	bool PersistenceBroker::DefaultAtomicReplace(
		const std::filesystem::path& temporary, const std::filesystem::path& destination)
	{
		return ReplaceFile(temporary, destination);
	}

	bool PersistenceBroker::SaveAtomic(const std::filesystem::path& path, std::uint32_t hostVersion,
		SaveStats* stats, const AtomicReplacer& replacer)
	{
        std::lock_guard fileLock(_fileMutex);
        try {
            const auto bytes = SaveToBytes(hostVersion, stats);
            std::string error;
            if (WriteAtomic(path, bytes, error, replacer)) return true;
            Log(LogLevel::kError, error);
        } catch (const std::exception& error) { Log(LogLevel::kError, error.what()); }
        return false;
    }

	PersistenceBroker::LoadStats PersistenceBroker::LoadFile(const std::filesystem::path& path,
		const FormResolver& resolver, bool revertFirst, bool* found)
	{
		std::lock_guard fileLock(_fileMutex);
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		if (!input) {
			if (found) {
				*found = false;
			}
			return LoadFromBytes({}, resolver, revertFirst);
		}
		if (found) {
			*found = true;
		}
		const auto end = input.tellg();
		if (end < 0 || static_cast<std::uint64_t>(end) > kMaxFileBytes) {
			Log(LogLevel::kError, "sidecar is unreadable or oversized");
			return LoadFromBytes({}, resolver, revertFirst);
		}
		std::vector<std::byte> bytes(static_cast<std::size_t>(end));
		input.seekg(0);
		if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
			Log(LogLevel::kError, "failed reading sidecar");
			return LoadFromBytes({}, resolver, revertFirst);
		}
		return LoadFromBytes(bytes, resolver, revertFirst);
	}
}
