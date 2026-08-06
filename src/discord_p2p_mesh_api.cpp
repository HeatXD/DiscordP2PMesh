#include "discord_p2p_mesh.h"
#include "session.h"

struct DPMeshSession : dpmesh::Session {
	using dpmesh::Session::Session;
};

DPMESH_API DPMeshSession *dpmesh_create(const DPMeshConfig *config) {
	return new DPMeshSession(config);
}

DPMESH_API void dpmesh_destroy(DPMeshSession *session) {
	delete session;
}

DPMESH_API void dpmesh_set_application_id(DPMeshSession *session, uint64_t application_id) {
	session->SetApplicationId(application_id);
}

DPMESH_API uint64_t dpmesh_get_application_id(DPMeshSession *session) {
	return session->GetApplicationId();
}

DPMESH_API void dpmesh_login(DPMeshSession *session) {
	session->Login();
}

DPMESH_API int dpmesh_is_ready(DPMeshSession *session) {
	return session->IsReady() ? 1 : 0;
}

DPMESH_API void dpmesh_create_or_join_lobby(DPMeshSession *session, const char *secret) {
	session->CreateOrJoinLobby(secret ? secret : "");
}

DPMESH_API void dpmesh_leave_lobby(DPMeshSession *session) {
	session->LeaveLobby();
}

DPMESH_API void dpmesh_send_lobby_message(DPMeshSession *session, const char *text) {
	session->SendLobbyMessage(text ? text : "");
}

DPMESH_API void dpmesh_send_to_peer(DPMeshSession *session, int64_t peer_id, const uint8_t *data, size_t size) {
	session->SendToPeer(peer_id, data, size);
}

DPMESH_API uint64_t dpmesh_get_current_user_id(DPMeshSession *session) {
	return session->GetCurrentUserId();
}

DPMESH_API const char *dpmesh_get_user_display_name(DPMeshSession *session, uint64_t user_id) {
	return session->GetUserDisplayName(user_id);
}

DPMESH_API const char *dpmesh_get_greeting(void) {
	static const std::string greeting = dpmesh::GetGreeting();
	return greeting.c_str();
}

DPMESH_API const char *dpmesh_get_discord_sdk_version(void) {
	static const std::string version = dpmesh::GetDiscordSdkVersion();
	return version.c_str();
}

DPMESH_API void dpmesh_update(DPMeshSession *session) {
	session->Update();
}

DPMESH_API int dpmesh_poll_event(DPMeshSession *session, DPMeshEvent *out_event) {
	return session->PollEvent(out_event) ? 1 : 0;
}
