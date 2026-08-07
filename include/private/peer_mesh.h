#ifndef DPMESH_PEER_MESH_H
#define DPMESH_PEER_MESH_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <juice/juice.h>

namespace dpmesh {

class Session;

struct IceServerConfig {
	std::string stun_host;
	uint16_t stun_port = 0;

	std::string turn_host;
	uint16_t turn_port = 0;
	std::string turn_username;
	std::string turn_password;
};

// Peer connections are libjuice ICE agents; offer/answer exchange rides on Session's lobby
// signaling (Session::SendSignalingMessage / HandleLobbyMessage).
//
// libjuice calls back from its own thread, so those callbacks just push a RawEvent onto a
// mutex-protected queue; PumpEvents() drains it on the caller's thread.
class PeerMesh {
public:
	PeerMesh(Session *owner, IceServerConfig ice_config);

	void SendToPeer(int64_t peer_id, const uint8_t *data, size_t size);
	void HandleSignaling(int64_t from_peer_id, bool is_offer, const std::string &sdp);

	// Tears down the ICE agent for a peer, if one exists. Returns true if it was connected,
	// so the caller knows whether a disconnect notification is owed.
	bool RemovePeer(int64_t peer_id);

	void PumpEvents();

private:
	struct JuiceAgentDeleter {
		void operator()(juice_agent_t *agent) const;
	};

	// Passed as user_ptr to libjuice so the static callbacks can recover self + peer_id.
	// &peer stays valid as long as it's in _peers: unordered_map only invalidates a
	// reference/pointer when that specific element is erased, never on insertion/rehash.
	struct Peer {
		PeerMesh *self = nullptr;
		int64_t peer_id = 0;
		std::unique_ptr<juice_agent_t, JuiceAgentDeleter> agent;
		bool is_offerer = false;
		bool connected = false;
	};

	struct RawEvent {
		enum class Kind { StateChanged,
			GatheringDone,
			Data } kind;
		int64_t peer_id = 0;
		juice_state_t state = JUICE_STATE_DISCONNECTED;
		std::vector<uint8_t> data;
	};

	Peer &GetOrCreatePeer(int64_t peer_id, bool is_offerer);
	void SendSignaling(int64_t to_peer_id, bool is_offer, const std::string &sdp);
	void DestroyPeer(std::unordered_map<int64_t, Peer>::iterator it);

	void OnPeerStateChanged(int64_t peer_id, juice_state_t state);
	void OnPeerGatheringDone(int64_t peer_id);
	void OnPeerData(int64_t peer_id, std::vector<uint8_t> data);

	static void JuiceStateChanged(juice_agent_t *agent, juice_state_t state, void *user_ptr);
	static void JuiceGatheringDone(juice_agent_t *agent, void *user_ptr);
	static void JuiceRecv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr);

	Session *_owner;
	IceServerConfig _ice_config;
	std::unordered_map<int64_t, Peer> _peers;

	std::mutex _raw_events_mutex;
	std::vector<RawEvent> _raw_events;
};

} // namespace dpmesh

#endif // DPMESH_PEER_MESH_H
