// ============================================================================
// OSFPersistenceAPI.h - copyable C ABI for OSF Settings per-save persistence.
//
// Drop this header into a native SFSE plugin and link nothing from OSF Settings.
// Acquire OSF_RequestPersistenceAPI with GetModuleHandleW/GetProcAddress, then
// register at SFSE kPostDataLoad. The host copies clientName and the descriptor.
// Callback tables and the returned service table are host-owned; never delete them.
//
// Compact example (error handling abbreviated):
//   auto mod = GetModuleHandleW(L"OSFSettings.dll");
//   auto request = reinterpret_cast<OSF_RequestPersistenceAPI_t>(
//       GetProcAddress(mod, "OSF_RequestPersistenceAPI"));
//   const OSFPersistenceAPI* api = request(OSF_PERSISTENCE_API_VERSION);
//   OSFPersistenceClientV1 client{};
//   client.size = sizeof(client);
//   client.clientID = OSF_PERSISTENCE_FOURCC('D','E','M','O');
//   client.clientName = "My Plugin";
//   client.context = &state;
//   client.save = SaveState; client.load = LoadState; client.revert = RevertState;
//   api->RegisterClient(api, &client);
//
// Callbacks are serialized, but can run off the game thread. They must not mutate
// engine state directly; marshal resolved results to SFSE's game-task queue.
// No callback may throw across this ABI (the host still catches defensively).
// ============================================================================

#pragma once

#include <stdint.h>

#if defined(_WIN32)
#  define OSF_PERSISTENCE_CALL __cdecl
#  if defined(OSF_PERSISTENCE_HOST)
#    define OSF_PERSISTENCE_EXPORT __declspec(dllexport)
#  else
#    define OSF_PERSISTENCE_EXPORT
#  endif
#else
#  define OSF_PERSISTENCE_CALL
#  define OSF_PERSISTENCE_EXPORT
#endif

#define OSF_PERSISTENCE_MAKE_VERSION(major, minor) ((((uint32_t)(major)) << 16u) | ((uint32_t)(minor) & 0xFFFFu))
#define OSF_PERSISTENCE_API_VERSION OSF_PERSISTENCE_MAKE_VERSION(1u, 0u)
#define OSF_PERSISTENCE_API_MAJOR(version) ((uint32_t)(version) >> 16u)
#define OSF_PERSISTENCE_API_MINOR(version) ((uint32_t)(version) & 0xFFFFu)
#define OSF_PERSISTENCE_FOURCC(a, b, c, d) \
	((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8u) | \
	 ((uint32_t)(uint8_t)(c) << 16u) | ((uint32_t)(uint8_t)(d) << 24u))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSFRecordWriterV1 OSFRecordWriterV1;
typedef struct OSFRecordReaderV1 OSFRecordReaderV1;
typedef struct OSFPersistenceClientV1 OSFPersistenceClientV1;
typedef struct OSFPersistenceAPI OSFPersistenceAPI;

struct OSFRecordWriterV1
{
	uint32_t size;
	void* context;
	uint8_t (OSF_PERSISTENCE_CALL *OpenRecord)(OSFRecordWriterV1* writer, uint32_t type, uint32_t version);
	uint8_t (OSF_PERSISTENCE_CALL *WriteRecordData)(OSFRecordWriterV1* writer, const void* data, uint32_t length);
};

struct OSFRecordReaderV1
{
	uint32_t size;
	void* context;
	uint8_t (OSF_PERSISTENCE_CALL *GetNextRecordInfo)(OSFRecordReaderV1* reader,
		uint32_t* outType, uint32_t* outVersion, uint32_t* outLength);
	uint32_t (OSF_PERSISTENCE_CALL *ReadRecordData)(OSFRecordReaderV1* reader, void* data, uint32_t length);
	uint8_t (OSF_PERSISTENCE_CALL *ResolveFormID)(OSFRecordReaderV1* reader,
		uint32_t oldFormID, uint32_t* outResolvedFormID);
};

struct OSFPersistenceClientV1
{
	uint32_t size;
	uint32_t clientID;       // Stable, globally unique FourCC-style identifier.
	const char* clientName;  // UTF-8 display name; copied during registration.
	void* context;           // Remains consumer-owned and must outlive registration.

	uint8_t (OSF_PERSISTENCE_CALL *save)(void* context, OSFRecordWriterV1* writer);
	uint8_t (OSF_PERSISTENCE_CALL *load)(void* context, OSFRecordReaderV1* reader);
	void (OSF_PERSISTENCE_CALL *revert)(void* context);
	void (OSF_PERSISTENCE_CALL *formDeleted)(void* context, uint32_t formID);
};

struct OSFPersistenceAPI
{
	uint32_t size;
	void* context;

	uint8_t (OSF_PERSISTENCE_CALL *IsReady)(const OSFPersistenceAPI* api);
	uint8_t (OSF_PERSISTENCE_CALL *RegisterClient)(const OSFPersistenceAPI* api,
		const OSFPersistenceClientV1* client);
	uint8_t (OSF_PERSISTENCE_CALL *UnregisterClient)(const OSFPersistenceAPI* api,
		uint32_t clientID, void* context);
	uint32_t (OSF_PERSISTENCE_CALL *GetInterfaceVersion)(const OSFPersistenceAPI* api);
	void (OSF_PERSISTENCE_CALL *GetPluginVersion)(const OSFPersistenceAPI* api,
		uint32_t* outMajor, uint32_t* outMinor, uint32_t* outPatch);
};

typedef const OSFPersistenceAPI* (OSF_PERSISTENCE_CALL *OSF_RequestPersistenceAPI_t)(uint32_t requestedVersion);

OSF_PERSISTENCE_EXPORT const OSFPersistenceAPI* OSF_PERSISTENCE_CALL
	OSF_RequestPersistenceAPI(uint32_t requestedVersion);


#ifdef __cplusplus
} // extern "C"
#endif
