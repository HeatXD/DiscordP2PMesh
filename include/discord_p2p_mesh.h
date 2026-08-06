#ifndef DISCORD_P2P_MESH_H
#define DISCORD_P2P_MESH_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#ifdef DPMESH_EXPORTS
#define DPMESH_API __declspec(dllexport)
#else
#define DPMESH_API __declspec(dllimport)
#endif
#else
#define DPMESH_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque session handle. One session owns one Discord client connection and its P2P mesh.
//
// Not thread-safe: call every dpmesh_* function for a given session from the same thread,
// the one driving dpmesh_update(). libjuice's own internal callback thread is the exception,
// already synchronized against internally.
typedef struct DPMeshSession DPMeshSession;

typedef struct DPMeshConfig {
	// Discord application (client) ID from the Developer Portal. Required.
	uint64_t application_id;

	// ICE servers used to establish the P2P mesh. stun_server_host may be NULL to disable STUN;
	// turn_server_host may be NULL to skip TURN entirely. Strings are copied by dpmesh_create().
	const char *stun_server_host;
	uint16_t stun_server_port;

	const char *turn_server_host;
	uint16_t turn_server_port;
	const char *turn_username;
	const char *turn_password;
} DPMeshConfig;

typedef enum DPMeshEventType {
	DPMESH_EVENT_NONE = 0,
	DPMESH_EVENT_READY,
	DPMESH_EVENT_AUTH_FAILED,
	DPMESH_EVENT_STATUS_CHANGED,
	DPMESH_EVENT_LOBBY_JOINED,
	DPMESH_EVENT_LOBBY_JOIN_FAILED,
	DPMESH_EVENT_LOBBY_LEFT,
	DPMESH_EVENT_LOBBY_MESSAGE,
	DPMESH_EVENT_LOBBY_MEMBER_JOINED,
	DPMESH_EVENT_LOBBY_MEMBER_LEFT,
	DPMESH_EVENT_PEER_CONNECTED,
	DPMESH_EVENT_PEER_DISCONNECTED,
	DPMESH_EVENT_PEER_DATA,
} DPMeshEventType;

typedef struct DPMeshStatusChangedEvent {
	int64_t status;
	int64_t error;
	int32_t error_detail;
} DPMeshStatusChangedEvent;

// Used for DPMESH_EVENT_AUTH_FAILED and DPMESH_EVENT_LOBBY_JOIN_FAILED.
typedef struct DPMeshMessageEvent {
	const char *message; // Valid until the next dpmesh_update() call on this session.
} DPMeshMessageEvent;

typedef struct DPMeshLobbyJoinedEvent {
	uint64_t lobby_id;
} DPMeshLobbyJoinedEvent;

typedef struct DPMeshLobbyMessageEvent {
	uint64_t from_user_id;
	const char *text; // Valid until the next dpmesh_update() call on this session.
} DPMeshLobbyMessageEvent;

typedef struct DPMeshLobbyMemberEvent {
	uint64_t member_id;
} DPMeshLobbyMemberEvent;

typedef struct DPMeshPeerConnEvent {
	int64_t peer_id;
} DPMeshPeerConnEvent;

typedef struct DPMeshPeerDataEvent {
	int64_t peer_id;
	const uint8_t *data; // Valid until the next dpmesh_update() call on this session.
	size_t size;
} DPMeshPeerDataEvent;

typedef struct DPMeshEvent {
	DPMeshEventType type;
	union {
		DPMeshStatusChangedEvent status_changed; // DPMESH_EVENT_STATUS_CHANGED
		DPMeshMessageEvent message; // DPMESH_EVENT_AUTH_FAILED, DPMESH_EVENT_LOBBY_JOIN_FAILED
		DPMeshLobbyJoinedEvent lobby_joined; // DPMESH_EVENT_LOBBY_JOINED
		DPMeshLobbyMessageEvent lobby_message; // DPMESH_EVENT_LOBBY_MESSAGE
		DPMeshLobbyMemberEvent lobby_member; // DPMESH_EVENT_LOBBY_MEMBER_JOINED, DPMESH_EVENT_LOBBY_MEMBER_LEFT
		DPMeshPeerConnEvent peer_conn; // DPMESH_EVENT_PEER_CONNECTED, DPMESH_EVENT_PEER_DISCONNECTED
		DPMeshPeerDataEvent peer_data; // DPMESH_EVENT_PEER_DATA
	};
} DPMeshEvent;

// application_id is required in *config; dpmesh_create() fails (returns NULL) without one.
DPMESH_API DPMeshSession *dpmesh_create(const DPMeshConfig *config);
DPMESH_API void dpmesh_destroy(DPMeshSession *session);

// Starts (or re-runs) the Discord OAuth + connect flow. Emits DPMESH_EVENT_READY on success or
// DPMESH_EVENT_AUTH_FAILED on failure via dpmesh_poll_event(), driven by dpmesh_update().
DPMESH_API void dpmesh_login(DPMeshSession *session);
DPMESH_API int dpmesh_is_ready(DPMeshSession *session);

DPMESH_API void dpmesh_create_or_join_lobby(DPMeshSession *session, const char *secret);
DPMESH_API void dpmesh_leave_lobby(DPMeshSession *session);
DPMESH_API void dpmesh_send_lobby_message(DPMeshSession *session, const char *text);

// First call per peer only starts the ICE connection; that call's data is not delivered.
DPMESH_API void dpmesh_send_to_peer(DPMeshSession *session, int64_t peer_id, const uint8_t *data, size_t size);

DPMESH_API uint64_t dpmesh_get_current_user_id(DPMeshSession *session);
// Returned pointer is valid until the next call to dpmesh_get_user_display_name() on this session.
DPMESH_API const char *dpmesh_get_user_display_name(DPMeshSession *session, uint64_t user_id);

DPMESH_API const char *dpmesh_get_discord_sdk_version(void);

// Pumps the Discord SDK and P2P mesh, queuing any resulting events. Call once per frame/tick;
// events from the previous dpmesh_update() call (and any pointers inside them) are invalidated here.
DPMESH_API void dpmesh_update(DPMeshSession *session);

// Drains one queued event per call, returning 1 and filling *out_event if one was available,
// 0 otherwise (out_event untouched).
DPMESH_API int dpmesh_poll_event(DPMeshSession *session, DPMeshEvent *out_event);

#ifdef __cplusplus
}
#endif

#endif // DISCORD_P2P_MESH_H
