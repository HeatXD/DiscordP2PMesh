#ifndef DPMESH_SESSION_H
#define DPMESH_SESSION_H

#include "peer_mesh.h"
#include "discord_p2p_mesh.h"

#include <discordpp.h>

#include <cstdint>
#include <string>
#include <vector>

namespace dpmesh {

// Backs one DPMeshSession handle. Queues DPMeshEvent structs for dpmesh_poll_event() instead
// of firing callbacks directly, so nothing here runs off the caller's own update thread.
class Session {
public:
	explicit Session(const DPMeshConfig *config);

	void Login();
	bool IsReady() const;

	void CreateOrJoinLobby(const std::string &secret);
	void LeaveLobby();
	void SendLobbyMessage(const std::string &text);

	void SendToPeer(int64_t peer_id, const uint8_t *data, size_t size);

	uint64_t GetCurrentUserId() const;
	const char *GetUserDisplayName(uint64_t user_id);

	void Update();
	bool PollEvent(DPMeshEvent *out_event);

	void SendSignalingMessage(int64_t to_peer_id, bool is_offer, const std::string &sdp);
	void QueuePeerConnected(int64_t peer_id);
	void QueuePeerDisconnected(int64_t peer_id);
	void QueuePeerData(int64_t peer_id, std::vector<uint8_t> data);

private:
	struct QueuedEvent {
		DPMeshEventType type = DPMESH_EVENT_NONE;
		int64_t status = 0;
		int64_t error = 0;
		int32_t error_detail = 0;
		uint64_t num_id = 0; // lobby_id / from_user_id / member_id, depending on type
		int64_t peer_id = 0;
		std::string text;
		std::vector<uint8_t> data;
	};

	bool EmitIfFailed(const discordpp::ClientResult &result, DPMeshEventType failure_event_type);
	void HandleLobbyMessage(uint64_t message_id);
	void SetRichPresenceState(const std::string &state);
	void QueueEvent(QueuedEvent event);

	uint64_t _application_id = 0;
	bool _callbacks_registered = false;
	discordpp::Client _client;
	uint64_t _lobby_id = 0;
	PeerMesh _mesh;

	std::vector<QueuedEvent> _events;
	size_t _poll_index = 0;
	std::string _display_name_scratch;
};

std::string GetDiscordSdkVersion();

} // namespace dpmesh

#endif // DPMESH_SESSION_H
