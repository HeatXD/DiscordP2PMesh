#include "peer_mesh.h"

#include "session.h"

#include <cstdio>
#include <cstring>

namespace dpmesh {

namespace {

const char *JuiceStateName(juice_state_t state) {
	switch (state) {
		case JUICE_STATE_DISCONNECTED:
			return "disconnected";
		case JUICE_STATE_GATHERING:
			return "gathering";
		case JUICE_STATE_CONNECTING:
			return "connecting";
		case JUICE_STATE_CONNECTED:
			return "connected";
		case JUICE_STATE_COMPLETED:
			return "completed";
		case JUICE_STATE_FAILED:
			return "failed";
		default:
			return "unknown";
	}
}

} // namespace

void PeerMesh::JuiceAgentDeleter::operator()(juice_agent_t *agent) const {
	if (agent) {
		juice_destroy(agent);
	}
}

PeerMesh::PeerMesh(Session *owner, IceServerConfig ice_config) :
		_owner(owner), _ice_config(std::move(ice_config)) {
}

void PeerMesh::SendToPeer(int64_t peer_id, const uint8_t *data, size_t size) {
	Peer &peer = GetOrCreatePeer(peer_id, /*is_offerer*/ true);
	if (!peer.connected || !peer.agent) {
		return;
	}

	juice_send(peer.agent.get(), (const char *)data, size);
}

PeerMesh::Peer &PeerMesh::GetOrCreatePeer(int64_t peer_id, bool is_offerer) {
	auto [it, inserted] = _peers.try_emplace(peer_id);
	Peer &peer = it->second;
	if (!inserted) {
		return peer;
	}

	peer.self = this;
	peer.peer_id = peer_id;
	peer.is_offerer = is_offerer;

	std::fprintf(stderr, "[ICE] peer %lld: creating agent (%s)\n", (long long)peer_id, is_offerer ? "offerer" : "answerer");

	juice_config_t config;
	memset(&config, 0, sizeof(config));
	if (!_ice_config.stun_host.empty()) {
		config.stun_server_host = _ice_config.stun_host.c_str();
		config.stun_server_port = _ice_config.stun_port;
	}

	juice_turn_server_t turn_server;
	memset(&turn_server, 0, sizeof(turn_server));
	if (!_ice_config.turn_host.empty()) {
		turn_server.host = _ice_config.turn_host.c_str();
		turn_server.port = _ice_config.turn_port;
		turn_server.username = _ice_config.turn_username.c_str();
		turn_server.password = _ice_config.turn_password.c_str();
		config.turn_servers = &turn_server;
		config.turn_servers_count = 1;
	}

	config.cb_state_changed = &PeerMesh::JuiceStateChanged;
	config.cb_gathering_done = &PeerMesh::JuiceGatheringDone;
	config.cb_recv = &PeerMesh::JuiceRecv;
	config.user_ptr = &peer;

	peer.agent.reset(juice_create(&config));
	juice_gather_candidates(peer.agent.get());
	return peer;
}

void PeerMesh::DestroyPeer(std::unordered_map<int64_t, Peer>::iterator it) {
	_peers.erase(it);
}

void PeerMesh::SendSignaling(int64_t to_peer_id, bool is_offer, const std::string &sdp) {
	_owner->SendSignalingMessage(to_peer_id, is_offer, sdp);
}

void PeerMesh::HandleSignaling(int64_t from_peer_id, bool is_offer, const std::string &sdp) {
	auto it = _peers.find(from_peer_id);
	std::fprintf(stderr, "[ICE] peer %lld: received %s\n", (long long)from_peer_id, is_offer ? "offer" : "answer");

	if (is_offer) {
		if (it != _peers.end() && it->second.is_offerer && !it->second.connected) {
			// Glare: both sides offered. Lower user ID stays the offerer; the other side
			// drops its agent and answers instead.
			if (_owner->GetCurrentUserId() < (uint64_t)from_peer_id) {
				std::fprintf(stderr, "[ICE] peer %lld: glare, keeping our own offer\n", (long long)from_peer_id);
				return;
			}
			std::fprintf(stderr, "[ICE] peer %lld: glare, dropping our offer to answer theirs\n", (long long)from_peer_id);
			DestroyPeer(it);
			it = _peers.end();
		}

		if (it != _peers.end()) {
			return;
		}

		Peer &peer = GetOrCreatePeer(from_peer_id, /*is_offerer*/ false);
		juice_set_remote_description(peer.agent.get(), sdp.c_str());
		juice_set_remote_gathering_done(peer.agent.get());
		return;
	}

	// Answer to our own outstanding offer.
	if (it == _peers.end() || !it->second.is_offerer) {
		std::fprintf(stderr, "[ICE] peer %lld: unexpected answer, ignoring\n", (long long)from_peer_id);
		return;
	}

	juice_set_remote_description(it->second.agent.get(), sdp.c_str());
	juice_set_remote_gathering_done(it->second.agent.get());
}

void PeerMesh::OnPeerStateChanged(int64_t peer_id, juice_state_t state) {
	auto it = _peers.find(peer_id);
	if (it == _peers.end()) {
		return;
	}

	std::fprintf(stderr, "[ICE] peer %lld: %s\n", (long long)peer_id, JuiceStateName(state));

	if (state == JUICE_STATE_CONNECTED || state == JUICE_STATE_COMPLETED) {
		if (!it->second.connected) {
			it->second.connected = true;
			_owner->QueuePeerConnected(peer_id);
		}
	} else if (state == JUICE_STATE_FAILED || state == JUICE_STATE_DISCONNECTED) {
		bool was_connected = it->second.connected;
		DestroyPeer(it);
		if (was_connected) {
			_owner->QueuePeerDisconnected(peer_id);
		}
	}
}

void PeerMesh::OnPeerGatheringDone(int64_t peer_id) {
	auto it = _peers.find(peer_id);
	if (it == _peers.end() || !it->second.agent) {
		return;
	}

	char sdp[JUICE_MAX_SDP_STRING_LEN];
	if (juice_get_local_description(it->second.agent.get(), sdp, sizeof(sdp)) != 0) {
		return;
	}

	std::fprintf(stderr, "[ICE] peer %lld: gathering done, sending %s\n", (long long)peer_id, it->second.is_offerer ? "offer" : "answer");
	SendSignaling(peer_id, it->second.is_offerer, std::string(sdp));
}

void PeerMesh::OnPeerData(int64_t peer_id, std::vector<uint8_t> data) {
	_owner->QueuePeerData(peer_id, std::move(data));
}

void PeerMesh::PumpEvents() {
	std::vector<RawEvent> events;
	{
		std::lock_guard<std::mutex> lock(_raw_events_mutex);
		events.swap(_raw_events);
	}

	for (RawEvent &event : events) {
		switch (event.kind) {
			case RawEvent::Kind::StateChanged:
				OnPeerStateChanged(event.peer_id, event.state);
				break;
			case RawEvent::Kind::GatheringDone:
				OnPeerGatheringDone(event.peer_id);
				break;
			case RawEvent::Kind::Data:
				OnPeerData(event.peer_id, std::move(event.data));
				break;
		}
	}
}

void PeerMesh::JuiceStateChanged(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
	Peer *peer = (Peer *)user_ptr;
	PeerMesh *mesh = peer->self;

	RawEvent event;
	event.kind = RawEvent::Kind::StateChanged;
	event.peer_id = peer->peer_id;
	event.state = state;

	std::lock_guard<std::mutex> lock(mesh->_raw_events_mutex);
	mesh->_raw_events.push_back(std::move(event));
}

void PeerMesh::JuiceGatheringDone(juice_agent_t *agent, void *user_ptr) {
	Peer *peer = (Peer *)user_ptr;
	PeerMesh *mesh = peer->self;

	RawEvent event;
	event.kind = RawEvent::Kind::GatheringDone;
	event.peer_id = peer->peer_id;

	std::lock_guard<std::mutex> lock(mesh->_raw_events_mutex);
	mesh->_raw_events.push_back(std::move(event));
}

void PeerMesh::JuiceRecv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
	Peer *peer = (Peer *)user_ptr;
	PeerMesh *mesh = peer->self;

	RawEvent event;
	event.kind = RawEvent::Kind::Data;
	event.peer_id = peer->peer_id;
	event.data.assign(data, data + size);

	std::lock_guard<std::mutex> lock(mesh->_raw_events_mutex);
	mesh->_raw_events.push_back(std::move(event));
}

} // namespace dpmesh
