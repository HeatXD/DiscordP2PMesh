#include "session.h"

#include <cstdio>

namespace dpmesh {

namespace {

constexpr const char *METADATA_KEY_TO = "dpmesh_to";
constexpr const char *METADATA_KEY_IS_OFFER = "dpmesh_offer";

void IgnoreSendResult(discordpp::ClientResult, uint64_t) {}

} // namespace

namespace {

IceServerConfig ToIceServerConfig(const DPMeshConfig *config) {
	IceServerConfig ice;
	if (!config) {
		return ice;
	}
	if (config->stun_server_host) {
		ice.stun_host = config->stun_server_host;
		ice.stun_port = config->stun_server_port;
	}
	if (config->turn_server_host) {
		ice.turn_host = config->turn_server_host;
		ice.turn_port = config->turn_server_port;
		ice.turn_username = config->turn_username ? config->turn_username : "";
		ice.turn_password = config->turn_password ? config->turn_password : "";
	}
	return ice;
}

} // namespace

Session::Session(const DPMeshConfig *config) :
		_mesh(this, ToIceServerConfig(config)) {
	if (config) {
		_application_id = config->application_id;
	}
}

bool Session::IsReady() const {
	return _client.GetStatus() == discordpp::Client::Status::Ready;
}

bool Session::EmitIfFailed(const discordpp::ClientResult &result, DPMeshEventType failure_event_type) {
	if (result.Successful()) {
		return false;
	}
	QueuedEvent event;
	event.type = failure_event_type;
	event.text = result.Error();
	QueueEvent(std::move(event));
	return true;
}

void Session::Login() {
	if (_application_id == 0) {
		std::fprintf(stderr, "dpmesh_login(): application_id is not set. Set it to your Discord application's client ID from the Developer Portal.\n");
		return;
	}

	if (!_callbacks_registered) {
		_callbacks_registered = true;

		_client.AddLogCallback(
				[](std::string message, discordpp::LoggingSeverity severity) {
					std::fprintf(stderr, "[Discord] %s\n", message.c_str());
				},
				discordpp::LoggingSeverity::Warning);

		_client.SetStatusChangedCallback([this](discordpp::Client::Status status, discordpp::Client::Error error, int32_t error_detail) {
			QueuedEvent event;
			event.type = DPMESH_EVENT_STATUS_CHANGED;
			event.status = (int64_t)status;
			event.error = (int64_t)error;
			event.error_detail = error_detail;
			QueueEvent(event);

			if (status == discordpp::Client::Status::Ready) {
				SetRichPresenceState("Idle");
				QueuedEvent ready;
				ready.type = DPMESH_EVENT_READY;
				QueueEvent(std::move(ready));
			} else if (error != discordpp::Client::Error::None) {
				QueuedEvent failed;
				failed.type = DPMESH_EVENT_AUTH_FAILED;
				failed.text = discordpp::Client::ErrorToString(error);
				QueueEvent(std::move(failed));
			}
		});

		_client.SetMessageCreatedCallback([this](uint64_t message_id) {
			HandleLobbyMessage(message_id);
		});

		_client.SetLobbyMemberAddedCallback([this](uint64_t lobby_id, uint64_t member_id) {
			if (lobby_id != _lobby_id || member_id == GetCurrentUserId()) {
				return;
			}
			QueuedEvent event;
			event.type = DPMESH_EVENT_LOBBY_MEMBER_JOINED;
			event.num_id = member_id;
			QueueEvent(std::move(event));
		});

		_client.SetLobbyMemberRemovedCallback([this](uint64_t lobby_id, uint64_t member_id) {
			if (lobby_id != _lobby_id) {
				return;
			}
			QueuedEvent event;
			event.type = DPMESH_EVENT_LOBBY_MEMBER_LEFT;
			event.num_id = member_id;
			QueueEvent(std::move(event));
		});
	}

	discordpp::AuthorizationCodeVerifier verifier = _client.CreateAuthorizationCodeVerifier();

	discordpp::AuthorizationArgs args;
	args.SetClientId(_application_id);
	args.SetScopes(discordpp::Client::GetDefaultCommunicationScopes());
	args.SetCodeChallenge(verifier.Challenge());

	_client.Authorize(args, [this, verifier](discordpp::ClientResult result, std::string code, std::string redirect_uri) {
		if (EmitIfFailed(result, DPMESH_EVENT_AUTH_FAILED)) {
			return;
		}

		_client.GetToken(
				_application_id, code, verifier.Verifier(), redirect_uri,
				[this](discordpp::ClientResult result, std::string access_token, std::string refresh_token,
						discordpp::AuthorizationTokenType token_type, int32_t expires_in, std::string scopes) {
					if (EmitIfFailed(result, DPMESH_EVENT_AUTH_FAILED)) {
						return;
					}

					_client.UpdateToken(
							discordpp::AuthorizationTokenType::Bearer, access_token,
							[this](discordpp::ClientResult result) {
								if (EmitIfFailed(result, DPMESH_EVENT_AUTH_FAILED)) {
									return;
								}
								_client.Connect();
							});
				});
	});
}

void Session::CreateOrJoinLobby(const std::string &secret) {
	if (!IsReady()) {
		std::fprintf(stderr, "dpmesh_create_or_join_lobby(): not connected yet, call dpmesh_login() and wait for DPMESH_EVENT_READY first.\n");
		return;
	}

	_client.CreateOrJoinLobby(secret, [this](discordpp::ClientResult result, uint64_t lobby_id) {
		if (EmitIfFailed(result, DPMESH_EVENT_LOBBY_JOIN_FAILED)) {
			return;
		}
		_lobby_id = lobby_id;
		SetRichPresenceState("In Lobby");

		QueuedEvent joined;
		joined.type = DPMESH_EVENT_LOBBY_JOINED;
		joined.num_id = lobby_id;
		QueueEvent(joined);

		// SetLobbyMemberAddedCallback only fires for members added after we start listening,
		// so without this, existing members would never be announced to us, only we to them.
		std::optional<discordpp::LobbyHandle> lobby = _client.GetLobbyHandle(lobby_id);
		if (lobby.has_value()) {
			for (uint64_t member_id : lobby->LobbyMemberIds()) {
				if (member_id != GetCurrentUserId()) {
					QueuedEvent member;
					member.type = DPMESH_EVENT_LOBBY_MEMBER_JOINED;
					member.num_id = member_id;
					QueueEvent(member);
				}
			}
		}
	});
}

void Session::LeaveLobby() {
	if (_lobby_id == 0) {
		return;
	}

	_client.LeaveLobby(_lobby_id, [this](discordpp::ClientResult result) {
		if (EmitIfFailed(result, DPMESH_EVENT_LOBBY_JOIN_FAILED)) {
			return;
		}
		_lobby_id = 0;
		SetRichPresenceState("Idle");
		QueuedEvent event;
		event.type = DPMESH_EVENT_LOBBY_LEFT;
		QueueEvent(std::move(event));
	});
}

void Session::SendLobbyMessage(const std::string &text) {
	if (_lobby_id == 0) {
		std::fprintf(stderr, "dpmesh_send_lobby_message(): not in a lobby, call dpmesh_create_or_join_lobby() first.\n");
		return;
	}

	_client.SendLobbyMessage(_lobby_id, text, &IgnoreSendResult);
}

void Session::SendToPeer(int64_t peer_id, const uint8_t *data, size_t size) {
	_mesh.SendToPeer(peer_id, data, size);
}

void Session::SendSignalingMessage(int64_t to_peer_id, bool is_offer, const std::string &sdp) {
	if (_lobby_id == 0) {
		return;
	}

	std::unordered_map<std::string, std::string> metadata;
	metadata[METADATA_KEY_TO] = std::to_string((uint64_t)to_peer_id);
	metadata[METADATA_KEY_IS_OFFER] = is_offer ? "1" : "0";

	_client.SendLobbyMessageWithMetadata(_lobby_id, sdp, metadata, &IgnoreSendResult);
}

uint64_t Session::GetCurrentUserId() const {
	std::optional<discordpp::UserHandle> user = _client.GetCurrentUserV2();
	return user.has_value() ? user->Id() : 0;
}

const char *Session::GetUserDisplayName(uint64_t user_id) {
	std::optional<discordpp::UserHandle> user = _client.GetUser(user_id);
	_display_name_scratch = user.has_value() ? user->DisplayName() : std::to_string(user_id);
	return _display_name_scratch.c_str();
}

void Session::HandleLobbyMessage(uint64_t message_id) {
	std::optional<discordpp::MessageHandle> message = _client.GetMessageHandle(message_id);
	if (!message.has_value()) {
		return;
	}

	uint64_t author_id = message->AuthorId();
	std::unordered_map<std::string, std::string> metadata = message->Metadata();
	auto to_it = metadata.find(METADATA_KEY_TO);

	if (to_it != metadata.end()) {
		if (std::stoull(to_it->second) != GetCurrentUserId()) {
			return;
		}

		bool is_offer = metadata[METADATA_KEY_IS_OFFER] == "1";
		_mesh.HandleSignaling((int64_t)author_id, is_offer, message->Content());
		return;
	}

	if (author_id == GetCurrentUserId()) {
		return; // Don't echo our own broadcast lobby messages back to ourselves.
	}

	QueuedEvent event;
	event.type = DPMESH_EVENT_LOBBY_MESSAGE;
	event.num_id = author_id;
	event.text = message->Content();
	QueueEvent(std::move(event));
}

void Session::SetRichPresenceState(const std::string &state) {
	discordpp::Activity activity;
	activity.SetType(discordpp::ActivityTypes::Playing);
	activity.SetState(state);
	activity.SetStatusDisplayType(discordpp::StatusDisplayTypes::State);

	_client.UpdateRichPresence(activity, [](discordpp::ClientResult result) {});
}

void Session::QueuePeerConnected(int64_t peer_id) {
	QueuedEvent event;
	event.type = DPMESH_EVENT_PEER_CONNECTED;
	event.peer_id = peer_id;
	QueueEvent(std::move(event));
}

void Session::QueuePeerDisconnected(int64_t peer_id) {
	QueuedEvent event;
	event.type = DPMESH_EVENT_PEER_DISCONNECTED;
	event.peer_id = peer_id;
	QueueEvent(std::move(event));
}

void Session::QueuePeerData(int64_t peer_id, std::vector<uint8_t> data) {
	QueuedEvent event;
	event.type = DPMESH_EVENT_PEER_DATA;
	event.peer_id = peer_id;
	event.data = std::move(data);
	QueueEvent(std::move(event));
}

void Session::QueueEvent(QueuedEvent event) {
	_events.push_back(std::move(event));
}

void Session::Update() {
	_events.clear(); // invalidates pointers from the previous frame's polled events, per API contract
	_poll_index = 0;

	discordpp::RunCallbacks();
	_mesh.PumpEvents();
}

bool Session::PollEvent(DPMeshEvent *out_event) {
	if (_poll_index >= _events.size()) {
		return false;
	}

	const QueuedEvent &src = _events[_poll_index++];
	out_event->type = src.type;

	switch (src.type) {
		case DPMESH_EVENT_STATUS_CHANGED:
			out_event->status_changed = { src.status, src.error, src.error_detail };
			break;
		case DPMESH_EVENT_AUTH_FAILED:
		case DPMESH_EVENT_LOBBY_JOIN_FAILED:
			out_event->message = { src.text.c_str() };
			break;
		case DPMESH_EVENT_LOBBY_JOINED:
			out_event->lobby_joined = { src.num_id };
			break;
		case DPMESH_EVENT_LOBBY_MESSAGE:
			out_event->lobby_message = { src.num_id, src.text.c_str() };
			break;
		case DPMESH_EVENT_LOBBY_MEMBER_JOINED:
		case DPMESH_EVENT_LOBBY_MEMBER_LEFT:
			out_event->lobby_member = { src.num_id };
			break;
		case DPMESH_EVENT_PEER_CONNECTED:
		case DPMESH_EVENT_PEER_DISCONNECTED:
			out_event->peer_conn = { src.peer_id };
			break;
		case DPMESH_EVENT_PEER_DATA:
			out_event->peer_data = { src.peer_id, src.data.data(), src.data.size() };
			break;
		case DPMESH_EVENT_READY:
		case DPMESH_EVENT_LOBBY_LEFT:
		case DPMESH_EVENT_NONE:
			break;
	}

	return true;
}

std::string GetDiscordSdkVersion() {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%d.%d.%d (%s)",
			discordpp::Client::GetVersionMajor(),
			discordpp::Client::GetVersionMinor(),
			discordpp::Client::GetVersionPatch(),
			discordpp::Client::GetVersionHash().c_str());
	return std::string(buf);
}

} // namespace dpmesh
