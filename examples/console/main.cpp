#include <discord_p2p_mesh.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> g_should_exit{ false };

static void HandleSigint(int) {
	g_should_exit = true;
}

// Reads lines from stdin on its own thread and hands them to the main loop through
// a small mutex-protected queue, so typing never blocks dpmesh_update().
static std::mutex g_input_mutex;
static std::vector<std::string> g_pending_lines;

static void StdinThread() {
	std::string line;
	while (std::getline(std::cin, line)) {
		if (line.empty()) {
			continue;
		}
		std::lock_guard<std::mutex> lock(g_input_mutex);
		g_pending_lines.push_back(std::move(line));
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <application_id> [lobby_secret]\n", argv[0]);
		return 1;
	}
	const std::string lobby_secret = argc > 2 ? argv[2] : "dpmesh-console-demo";

	signal(SIGINT, HandleSigint);

	std::thread input_thread(StdinThread);
	input_thread.detach();
	printf("Type a message and press Enter to chat in the lobby, or \"/p <message>\" to send it to every connected peer over P2P.\n");

	DPMeshConfig config;
	memset(&config, 0, sizeof(config));
	config.application_id = strtoull(argv[1], nullptr, 10);
	config.stun_server_host = "stun.l.google.com";
	config.stun_server_port = 19302;

	DPMeshSession *session = dpmesh_create(&config);
	if (!session) {
		fprintf(stderr, "dpmesh_create failed: application_id must be non-zero\n");
		return 1;
	}

	printf("Discord SDK version: %s\n", dpmesh_get_discord_sdk_version());
	dpmesh_login(session);

	std::vector<int64_t> connected_peers;

	while (!g_should_exit) {
		dpmesh_update(session);

		DPMeshEvent event;
		while (dpmesh_poll_event(session, &event)) {
			switch (event.type) {
				case DPMESH_EVENT_READY: {
					uint64_t user_id = dpmesh_get_current_user_id(session);
					printf("ready as %s (id %llu, dpmesh_is_ready=%d), joining lobby '%s'\n",
							dpmesh_get_user_display_name(session, user_id), (unsigned long long)user_id,
							dpmesh_is_ready(session), lobby_secret.c_str());
					dpmesh_create_or_join_lobby(session, lobby_secret.c_str());
					break;
				}
				case DPMESH_EVENT_AUTH_FAILED:
					printf("auth failed: %s\n", event.message.message);
					break;
				case DPMESH_EVENT_STATUS_CHANGED:
					printf("status changed: status=%lld error=%lld detail=%d\n",
							(long long)event.status_changed.status,
							(long long)event.status_changed.error,
							event.status_changed.error_detail);
					break;
				case DPMESH_EVENT_LOBBY_JOINED:
					printf("joined lobby %llu\n", (unsigned long long)event.lobby_joined.lobby_id);
					dpmesh_send_lobby_message(session, "hello from the console example");
					break;
				case DPMESH_EVENT_LOBBY_JOIN_FAILED:
					printf("lobby join failed: %s\n", event.message.message);
					break;
				case DPMESH_EVENT_LOBBY_LEFT:
					printf("left the lobby\n");
					break;
				case DPMESH_EVENT_LOBBY_MESSAGE:
					printf("%s: %s\n", dpmesh_get_user_display_name(session, event.lobby_message.from_user_id), event.lobby_message.text);
					break;
				case DPMESH_EVENT_LOBBY_MEMBER_JOINED:
					printf("%s joined the lobby, starting a P2P connection\n", dpmesh_get_user_display_name(session, event.lobby_member.member_id));
					dpmesh_send_to_peer(session, (int64_t)event.lobby_member.member_id, (const uint8_t *)"hi", 2); // first call only starts the ICE connection
					break;
				case DPMESH_EVENT_LOBBY_MEMBER_LEFT:
					printf("%s left the lobby\n", dpmesh_get_user_display_name(session, event.lobby_member.member_id));
					break;
				case DPMESH_EVENT_PEER_CONNECTED:
					printf("%s connected over P2P, sending a message\n", dpmesh_get_user_display_name(session, (uint64_t)event.peer_conn.peer_id));
					connected_peers.push_back(event.peer_conn.peer_id);
					dpmesh_send_to_peer(session, event.peer_conn.peer_id, (const uint8_t *)"hello over P2P", 15);
					break;
				case DPMESH_EVENT_PEER_DISCONNECTED:
					printf("%s disconnected\n", dpmesh_get_user_display_name(session, (uint64_t)event.peer_conn.peer_id));
					connected_peers.erase(std::remove(connected_peers.begin(), connected_peers.end(), event.peer_conn.peer_id), connected_peers.end());
					break;
				case DPMESH_EVENT_PEER_DATA:
					printf("peer %lld sent %zu bytes: %.*s\n", (long long)event.peer_data.peer_id,
							event.peer_data.size, (int)event.peer_data.size, (const char *)event.peer_data.data);
					break;
				case DPMESH_EVENT_NONE:
					break;
			}
		}

		std::vector<std::string> lines;
		{
			std::lock_guard<std::mutex> lock(g_input_mutex);
			lines.swap(g_pending_lines);
		}
		for (const std::string &line : lines) {
			if (line.rfind("/p ", 0) == 0) {
				if (connected_peers.empty()) {
					printf("no connected peers yet\n");
					continue;
				}
				const std::string message = line.substr(3);
				for (int64_t peer_id : connected_peers) {
					dpmesh_send_to_peer(session, peer_id, (const uint8_t *)message.data(), message.size());
				}
			} else {
				dpmesh_send_lobby_message(session, line.c_str());
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	printf("shutting down...\n");
	dpmesh_leave_lobby(session);
	dpmesh_update(session);

	DPMeshEvent event;
	while (dpmesh_poll_event(session, &event)) {
		if (event.type == DPMESH_EVENT_LOBBY_LEFT) {
			printf("left the lobby\n");
		}
	}

	dpmesh_destroy(session);
	return 0;
}
