#include <discord_p2p_mesh.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#include <unistd.h>
static void sleep_ms(int ms) { usleep((useconds_t)ms * 1000); }
#endif

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <application_id>\n", argv[0]);
		return 1;
	}

	DPMeshConfig config;
	memset(&config, 0, sizeof(config));
	config.application_id = strtoull(argv[1], NULL, 10);
	config.stun_server_host = "stun.l.google.com";
	config.stun_server_port = 19302;

	DPMeshSession *session = dpmesh_create(&config);
	printf("%s\n", dpmesh_get_greeting());
	printf("Discord SDK version: %s\n", dpmesh_get_discord_sdk_version());

	dpmesh_login(session);

	for (;;) {
		dpmesh_update(session);

		DPMeshEvent event;
		while (dpmesh_poll_event(session, &event)) {
			switch (event.type) {
				case DPMESH_EVENT_READY:
					printf("ready! user id: %llu\n", (unsigned long long)dpmesh_get_current_user_id(session));
					break;
				case DPMESH_EVENT_AUTH_FAILED:
					printf("auth failed: %s\n", event.message.message);
					break;
				case DPMESH_EVENT_STATUS_CHANGED:
					printf("status changed: status=%lld error=%lld detail=%d\n",
							(long long)event.status_changed.status,
							(long long)event.status_changed.error,
							event.status_changed.error_detail);
					break;
				default:
					break;
			}
		}

		sleep_ms(16);
	}

	dpmesh_destroy(session);
	return 0;
}
