#include <discord_p2p_mesh.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static volatile sig_atomic_t g_should_exit = 0;

static void HandleSigint(int sig) {
	(void)sig;
	g_should_exit = 1;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <application_id> [lobby_secret]\n", argv[0]);
		return 1;
	}
	const char *lobby_secret = argc > 2 ? argv[2] : "dpmesh-console-demo";

	signal(SIGINT, HandleSigint);

	DPMeshConfig config;
	memset(&config, 0, sizeof(config));
	config.application_id = strtoull(argv[1], NULL, 10);
	config.stun_server_host = "stun.l.google.com";
	config.stun_server_port = 19302;

	DPMeshSession *session = dpmesh_create(&config);
	if (!session) {
		fprintf(stderr, "dpmesh_create failed: application_id must be non-zero\n");
		return 1;
	}

	printf("Discord SDK version: %s\n", dpmesh_get_discord_sdk_version());
	dpmesh_login(session);

	int64_t chat_peer_id = 0;

	while (!g_should_exit) {
		dpmesh_update(session);

		DPMeshEvent event;
		while (dpmesh_poll_event(session, &event)) {
			switch (event.type) {
				case DPMESH_EVENT_READY: {
					uint64_t user_id = dpmesh_get_current_user_id(session);
					printf("ready as %s (id %llu, dpmesh_is_ready=%d), joining lobby '%s'\n",
							dpmesh_get_user_display_name(session, user_id), (unsigned long long)user_id,
							dpmesh_is_ready(session), lobby_secret);
					dpmesh_create_or_join_lobby(session, lobby_secret);
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
					if (chat_peer_id == 0) {
						chat_peer_id = (int64_t)event.lobby_message.from_user_id;
						printf("starting a P2P connection to %s\n", dpmesh_get_user_display_name(session, event.lobby_message.from_user_id));
						dpmesh_send_to_peer(session, chat_peer_id, (const uint8_t *)"hi", 2); // first call only starts the ICE connection
					}
					break;
				case DPMESH_EVENT_LOBBY_MEMBER_JOINED:
					printf("%s joined the lobby\n", dpmesh_get_user_display_name(session, event.lobby_member.member_id));
					break;
				case DPMESH_EVENT_LOBBY_MEMBER_LEFT:
					printf("%s left the lobby\n", dpmesh_get_user_display_name(session, event.lobby_member.member_id));
					break;
				case DPMESH_EVENT_PEER_CONNECTED:
					printf("peer %lld connected, sending a P2P message\n", (long long)event.peer_conn.peer_id);
					dpmesh_send_to_peer(session, event.peer_conn.peer_id, (const uint8_t *)"hello over P2P", 15);
					break;
				case DPMESH_EVENT_PEER_DISCONNECTED:
					printf("peer %lld disconnected\n", (long long)event.peer_conn.peer_id);
					break;
				case DPMESH_EVENT_PEER_DATA:
					printf("peer %lld sent %zu bytes: %.*s\n", (long long)event.peer_data.peer_id,
							event.peer_data.size, (int)event.peer_data.size, (const char *)event.peer_data.data);
					break;
				case DPMESH_EVENT_NONE:
					break;
			}
		}

		Sleep(16);
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
