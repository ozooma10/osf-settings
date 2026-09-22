#include "PersistenceCheck.h"

#include "Persistence/PersistenceBroker.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using OSFSettings::Persistence::PersistenceBroker;

namespace
{
	constexpr std::uint32_t A = OSF_PERSISTENCE_FOURCC('A', 'A', 'A', 'A');
	constexpr std::uint32_t B = OSF_PERSISTENCE_FOURCC('B', 'B', 'B', 'B');
	constexpr std::uint32_t U = OSF_PERSISTENCE_FOURCC('U', 'N', 'K', 'N');
	constexpr std::uint32_t R1 = OSF_PERSISTENCE_FOURCC('R', 'E', 'C', '1');
	constexpr std::uint32_t R2 = OSF_PERSISTENCE_FOURCC('R', 'E', 'C', '2');
	constexpr std::uint32_t RX = OSF_PERSISTENCE_FOURCC('U', 'N', 'K', 'R');

	struct Record
	{
		std::uint32_t type{};
		std::uint32_t version{};
		std::vector<std::uint8_t> data;
	};

	struct State
	{
		std::string label;
		std::vector<Record> saveRecords;
		std::vector<Record> loadedRecords;
		std::vector<std::uint32_t> seenTypes;
		std::set<std::uint32_t> acceptedTypes;
		std::vector<std::string>* events{};
		bool saveResult{ true };
		bool loadResult{ true };
		bool throwSave{};
		bool throwLoad{};
		bool partialFirst{};
		std::uint32_t resolveOld{};
		std::uint32_t resolved{};
		bool resolveOK{};
		int reverts{};
		std::vector<std::uint32_t> deleted;
	};

	std::uint8_t OSF_PERSISTENCE_CALL Save(void* context, OSFRecordWriterV1* writer)
	{
		auto& state = *static_cast<State*>(context);
		if (state.throwSave) {
			throw std::runtime_error("save failure");
		}
		for (const auto& record : state.saveRecords) {
			if (!writer->OpenRecord(writer, record.type, record.version) ||
				!writer->WriteRecordData(writer, record.data.data(), static_cast<std::uint32_t>(record.data.size()))) {
				return 0;
			}
		}
		return state.saveResult;
	}

	std::uint8_t OSF_PERSISTENCE_CALL Load(void* context, OSFRecordReaderV1* reader)
	{
		auto& state = *static_cast<State*>(context);
		if (state.events) {
			state.events->push_back("load:" + state.label);
		}
		if (state.throwLoad) {
			throw std::runtime_error("load failure");
		}
		std::uint32_t type{}, version{}, length{};
		std::size_t index = 0;
		while (reader->GetNextRecordInfo(reader, &type, &version, &length)) {
			state.seenTypes.push_back(type);
			const bool accept = state.acceptedTypes.empty() || state.acceptedTypes.contains(type);
			if (!accept) {
				++index;
				continue;
			}
			Record record{ type, version, {} };
			const auto wanted = state.partialFirst && index == 0 ? std::min<std::uint32_t>(1, length) : length;
			record.data.resize(wanted);
			if (wanted && reader->ReadRecordData(reader, record.data.data(), wanted) != wanted) {
				return 0;
			}
			state.loadedRecords.push_back(std::move(record));
			++index;
		}
		if (state.resolveOld) {
			state.resolveOK = reader->ResolveFormID(reader, state.resolveOld, &state.resolved) != 0;
		}
		return state.loadResult;
	}

	void OSF_PERSISTENCE_CALL Revert(void* context)
	{
		auto& state = *static_cast<State*>(context);
		++state.reverts;
		if (state.events) {
			state.events->push_back("revert:" + state.label);
		}
	}

	void OSF_PERSISTENCE_CALL Deleted(void* context, std::uint32_t formID)
	{
		static_cast<State*>(context)->deleted.push_back(formID);
	}

	OSFPersistenceClientV1 Client(std::uint32_t id, const char* name, State& state)
	{
		return { sizeof(OSFPersistenceClientV1), id, name, &state, &Save, &Load, &Revert, &Deleted };
	}

	std::uint32_t ReadU32(const std::vector<std::byte>& bytes, std::size_t offset)
	{
		if (!CHECK(offset <= bytes.size() && bytes.size() - offset >= 4)) {
			return 0;
		}
		return static_cast<std::uint32_t>(bytes[offset]) |
			(static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
			(static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
			(static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
	}

	void PatchU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
	{
		if (!CHECK(offset <= bytes.size() && bytes.size() - offset >= 4)) {
			return;
		}
		for (std::size_t i = 0; i < 4; ++i) {
			bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFF);
		}
	}

	std::uint32_t CRC32(const std::byte* data, std::size_t length)
	{
		std::uint32_t crc = 0xFFFFFFFFu;
		for (std::size_t i = 0; i < length; ++i) {
			crc ^= static_cast<std::uint8_t>(data[i]);
			for (int bit = 0; bit < 8; ++bit) {
				crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
			}
		}
		return ~crc;
	}

	std::vector<std::byte> MakeTwoClientFile(State& first, State& second)
	{
		PersistenceBroker producer;
		CHECK(producer.RegisterClient(Client(A, "A", first)));
		CHECK(producer.RegisterClient(Client(B, "B", second)));
		return producer.SaveToBytes(0x010100);
	}

	void TestRoundTripAndRecords()
	{
		State a{ .label = "A", .saveRecords = { { R1, 1, { 1, 2, 3 } }, { R2, 7, { 4, 5 } } } };
		State b{ .label = "B", .saveRecords = { { R2, 2, { 9 } } } };
		auto bytes = MakeTwoClientFile(a, b);
		CHECK(ReadU32(bytes, 16) == 2);

		State loadedA{ .label = "A" };
		State loadedB{ .label = "B" };
		PersistenceBroker consumer;
		CHECK(consumer.RegisterClient(Client(A, "A", loadedA)));
		CHECK(consumer.RegisterClient(Client(B, "B", loadedB)));
		const auto stats = consumer.LoadFromBytes(bytes);
		CHECK(stats.clientsLoaded == 2);
		if (CHECK(loadedA.loadedRecords.size() == 2)) {
			CHECK(loadedA.loadedRecords[0].data == std::vector<std::uint8_t>({ 1, 2, 3 }));
			CHECK(loadedA.loadedRecords[1].version == 7);
		}
		if (CHECK(loadedB.loadedRecords.size() == 1)) {
			CHECK(loadedB.loadedRecords[0].data == std::vector<std::uint8_t>({ 9 }));
		}
		CHECK(loadedA.reverts == 1 && loadedB.reverts == 1);
	}

	void TestEmptyAndDuplicate()
	{
		State empty{ .label = "empty" };
		PersistenceBroker broker;
		CHECK(broker.RegisterClient(Client(A, "A", empty)));
		CHECK(!broker.RegisterClient(Client(A, "duplicate", empty)));
		const auto bytes = broker.SaveToBytes(1);
		CHECK(bytes.size() == 24);
		CHECK(ReadU32(bytes, 16) == 0);
	}

	void TestUnknownClient()
	{
		State unknown{ .label = "unknown", .saveRecords = { { R1, 1, { 3 } } } };
		PersistenceBroker producer;
		CHECK(producer.RegisterClient(Client(U, "unknown", unknown)));
		const auto bytes = producer.SaveToBytes(1);
		State known{ .label = "known" };
		PersistenceBroker consumer;
		CHECK(consumer.RegisterClient(Client(B, "known", known)));
		const auto stats = consumer.LoadFromBytes(bytes);
		CHECK(stats.unknownClients == 1 && stats.clientsLoaded == 0);
		CHECK(known.reverts == 1);
	}

	void TestUnknownRecordAndUnreadTail()
	{
		State source{ .label = "source", .saveRecords = {
			{ R1, 1, { 1, 2, 3, 4 } }, { RX, 1, { 8, 8 } }, { R2, 1, { 5, 6 } }
		} };
		PersistenceBroker producer;
		CHECK(producer.RegisterClient(Client(A, "A", source)));
		const auto bytes = producer.SaveToBytes(1);

		State target{ .label = "target", .acceptedTypes = { R1, R2 }, .partialFirst = true };
		PersistenceBroker consumer;
		CHECK(consumer.RegisterClient(Client(A, "A", target)));
		CHECK(consumer.LoadFromBytes(bytes).clientsLoaded == 1);
		CHECK(target.seenTypes == std::vector<std::uint32_t>({ R1, RX, R2 }));
		if (CHECK(target.loadedRecords.size() == 2)) {
			CHECK(target.loadedRecords[0].data == std::vector<std::uint8_t>({ 1 }));
			CHECK(target.loadedRecords[1].data == std::vector<std::uint8_t>({ 5, 6 }));
		}
	}

	void TestTruncationAndCaps()
	{
		State source{ .label = "source", .saveRecords = { { R1, 1, { 1, 2, 3 } } } };
		PersistenceBroker producer;
		CHECK(producer.RegisterClient(Client(A, "A", source)));
		const auto good = producer.SaveToBytes(1);
		State target{ .label = "target" };
		PersistenceBroker consumer;
		CHECK(consumer.RegisterClient(Client(A, "A", target)));

		auto truncatedHeader = good;
		truncatedHeader.resize(10);
		CHECK(consumer.LoadFromBytes(truncatedHeader).clientsLoaded == 0);
		auto truncatedClient = good;
		truncatedClient.resize(30);
		CHECK(consumer.LoadFromBytes(truncatedClient).clientsLoaded == 0);
		auto truncatedPayload = good;
		if (!CHECK(!truncatedPayload.empty())) {
			return;
		}
		truncatedPayload.pop_back();
		CHECK(consumer.LoadFromBytes(truncatedPayload).clientsLoaded == 0);

		auto tooManyClients = good;
		PatchU32(tooManyClients, 16, PersistenceBroker::kMaxClients + 1);
		CHECK(consumer.LoadFromBytes(tooManyClients).clientsLoaded == 0);
		auto tooManyRecords = good;
		PatchU32(tooManyRecords, 28, PersistenceBroker::kMaxRecordsPerClient + 1);
		CHECK(consumer.LoadFromBytes(tooManyRecords).corruptBlocks == 1);
		auto oversizedRecord = good;
		const auto payloadLength = ReadU32(oversizedRecord, 32);
		PatchU32(oversizedRecord, 52, PersistenceBroker::kMaxRecordBytes + 1);
		if (!CHECK(oversizedRecord.size() >= 44 &&
			payloadLength <= oversizedRecord.size() - 44)) {
			return;
		}
		PatchU32(oversizedRecord, 36, CRC32(oversizedRecord.data() + 44, payloadLength));
		CHECK(consumer.LoadFromBytes(oversizedRecord).corruptBlocks == 1);
	}

	void TestCorruptThenValid()
	{
		State a{ .label = "A", .saveRecords = { { R1, 1, { 1 } } } };
		State b{ .label = "B", .saveRecords = { { R2, 1, { 2 } } } };
		auto bytes = MakeTwoClientFile(a, b);
		PatchU32(bytes, 36, ReadU32(bytes, 36) ^ 1u);

		State loadedA{ .label = "A" };
		State loadedB{ .label = "B" };
		PersistenceBroker consumer;
		CHECK(consumer.RegisterClient(Client(A, "A", loadedA)));
		CHECK(consumer.RegisterClient(Client(B, "B", loadedB)));
		const auto stats = consumer.LoadFromBytes(bytes);
		CHECK(stats.corruptBlocks == 1 && stats.clientsLoaded == 1);
		CHECK(loadedA.loadedRecords.empty());
		if (CHECK(loadedB.loadedRecords.size() == 1)) {
			CHECK(loadedB.loadedRecords[0].data == std::vector<std::uint8_t>({ 2 }));
		}
	}

	void TestCallbackIsolation()
	{
		State badSave{ .label = "bad-save", .saveRecords = { { R1, 1, { 1 } } }, .saveResult = false };
		State goodSave{ .label = "good-save", .saveRecords = { { R2, 1, { 2 } } } };
		PersistenceBroker producer;
		CHECK(producer.RegisterClient(Client(A, "A", badSave)));
		CHECK(producer.RegisterClient(Client(B, "B", goodSave)));
		PersistenceBroker::SaveStats saveStats;
		const auto bytes = producer.SaveToBytes(1, &saveStats);
		CHECK(saveStats.clientsFailed == 1 && saveStats.clientsWritten == 1);

		State badLoad{ .label = "bad-load", .loadResult = false };
		State goodLoad{ .label = "good-load" };
		PersistenceBroker consumer;
		CHECK(consumer.RegisterClient(Client(A, "A", badLoad)));
		CHECK(consumer.RegisterClient(Client(B, "B", goodLoad)));
		// A was omitted by its failed save; produce a valid two-client file for load isolation.
		badSave.saveResult = true;
		const auto two = producer.SaveToBytes(1);
		const auto stats = consumer.LoadFromBytes(two);
		CHECK(stats.callbackFailures == 1 && stats.clientsLoaded == 1);
		CHECK(goodLoad.loadedRecords.size() == 1);

		badLoad.loadResult = true;
		badLoad.throwLoad = true;
		goodLoad.loadedRecords.clear();
		const auto throwLoadStats = consumer.LoadFromBytes(two);
		CHECK(throwLoadStats.callbackFailures == 1 && throwLoadStats.clientsLoaded == 1);
		CHECK(goodLoad.loadedRecords.size() == 1);

		State throwSave{ .label = "throw-save", .throwSave = true };
		State throwGood{ .label = "throw-good", .saveRecords = { { R2, 1, { 4 } } } };
		PersistenceBroker throws;
		CHECK(throws.RegisterClient(Client(A, "A", throwSave)));
		CHECK(throws.RegisterClient(Client(B, "B", throwGood)));
		PersistenceBroker::SaveStats throwStats;
		CHECK(ReadU32(throws.SaveToBytes(1, &throwStats), 16) == 1);
		CHECK(throwStats.clientsFailed == 1);
	}

	void TestRevertResolverAndDeleted()
	{
		State source{ .label = "source", .saveRecords = { { R1, 1, { 1 } } } };
		PersistenceBroker producer;
		CHECK(producer.RegisterClient(Client(A, "A", source)));
		const auto bytes = producer.SaveToBytes(1);

		std::vector<std::string> events;
		State target{ .label = "A", .events = &events, .resolveOld = 0x1234 };
		PersistenceBroker consumer;
		CHECK(consumer.RegisterClient(Client(A, "A", target)));
		const auto stats = consumer.LoadFromBytes(bytes,
			[](std::uint32_t oldID, std::uint32_t& resolved) {
				if (oldID != 0x1234) return false;
				resolved = 0x5678;
				return true;
			});
		CHECK(stats.clientsLoaded == 1);
		CHECK(events == std::vector<std::string>({ "revert:A", "load:A" }));
		CHECK(target.resolveOK && target.resolved == 0x5678);
		consumer.NotifyFormDeleted(0xCAFE);
		CHECK(target.deleted == std::vector<std::uint32_t>({ 0xCAFE }));
	}

	void TestDeterministicOrdering()
	{
		State a1{ .label = "A", .saveRecords = { { R1, 1, { 1 } } } };
		State b1{ .label = "B", .saveRecords = { { R2, 1, { 2 } } } };
		State a2 = a1;
		State b2 = b1;
		PersistenceBroker first;
		PersistenceBroker second;
		CHECK(first.RegisterClient(Client(B, "B", b1)));
		CHECK(first.RegisterClient(Client(A, "A", a1)));
		CHECK(second.RegisterClient(Client(A, "A", a2)));
		CHECK(second.RegisterClient(Client(B, "B", b2)));
		CHECK(first.SaveToBytes(7) == second.SaveToBytes(7));
	}

	void TestAtomicFailurePreservesOld()
	{
		const auto directory = std::filesystem::current_path() / "build" / "persistence-test";
		const auto path = directory / "atomic.osf";
		std::filesystem::create_directories(directory);
		{
			std::ofstream old(path, std::ios::binary | std::ios::trunc);
			old << "previous-valid-data";
		}
		State state{ .label = "A", .saveRecords = { { R1, 1, { 1 } } } };
		PersistenceBroker broker;
		CHECK(broker.RegisterClient(Client(A, "A", state)));
		CHECK(!broker.SaveAtomic(path, 1, nullptr,
			[](const std::filesystem::path&, const std::filesystem::path&) { return false; }));
		std::ifstream input(path, std::ios::binary);
		const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		CHECK(contents == "previous-valid-data");
		CHECK(!std::filesystem::exists(path.wstring() + L".tmp"));
		std::error_code ec;
		std::filesystem::remove_all(directory, ec);
	}
}

int main()
{
	try {
		TestRoundTripAndRecords();
		TestEmptyAndDuplicate();
		TestUnknownClient();
		TestUnknownRecordAndUnreadTail();
		TestTruncationAndCaps();
		TestCorruptThenValid();
		TestCallbackIsolation();
		TestRevertResolverAndDeleted();
		TestDeterministicOrdering();
		TestAtomicFailurePreservesOld();
	} catch (const std::exception& error) {
		OSF::Test::Check(false, error.what());
	} catch (...) {
		OSF::Test::Check(false, "unexpected non-standard exception");
	}
	return OSF::Test::Finish("Persistence");
}
