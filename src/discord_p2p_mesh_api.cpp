#include "discord_p2p_mesh.h"
#include "session.h"

struct DPMeshSession : dpmesh::Session {
	using dpmesh::Session::Session;
};

DPMESH_API DPMeshSession *dpmesh_create(const DPMeshConfig *config) {
	if (!config || config->application_id == 0) {
		return nullptr;
	}
	return new DPMeshSession(config);
}

DPMESH_API void dpmesh_destroy(DPMeshSession *session) {
	delete session;
}

DPMESH_API void dpmesh_login(DPMeshSession *session) {
	if (session) {
		session->Login();
	}
}

DPMESH_API int dpmesh_is_ready(DPMeshSession *session) {
	return session && session->IsReady() ? 1 : 0;
}

DPMESH_API void dpmesh_create_or_join_lobby(DPMeshSession *session, const char *secret) {
	if (session) {
		session->CreateOrJoinLobby(secret ? secret : "");
	}
}

DPMESH_API void dpmesh_leave_lobby(DPMeshSession *session) {
	if (session) {
		session->LeaveLobby();
	}
}

DPMESH_API void dpmesh_send_lobby_message(DPMeshSession *session, const char *text) {
	if (session) {
		session->SendLobbyMessage(text ? text : "");
	}
}

DPMESH_API void dpmesh_send_to_peer(DPMeshSession *session, int64_t peer_id, const uint8_t *data, size_t size) {
	if (session) {
		session->SendToPeer(peer_id, data, size);
	}
}

DPMESH_API uint64_t dpmesh_get_current_user_id(DPMeshSession *session) {
	return session ? session->GetCurrentUserId() : 0;
}

DPMESH_API const char *dpmesh_get_user_display_name(DPMeshSession *session, uint64_t user_id) {
	static const std::string fallback;
	return session ? session->GetUserDisplayName(user_id) : fallback.c_str();
}

DPMESH_API const char *dpmesh_get_discord_sdk_version(void) {
	static const std::string version = dpmesh::GetDiscordSdkVersion();
	return version.c_str();
}

DPMESH_API void dpmesh_update(DPMeshSession *session) {
	if (session) {
		session->Update();
	}
}

DPMESH_API int dpmesh_poll_event(DPMeshSession *session, DPMeshEvent *out_event) {
	return session && session->PollEvent(out_event) ? 1 : 0;
}
