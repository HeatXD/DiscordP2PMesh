# DiscordP2PMesh

Engine-agnostic C++ library exposing a plain C API for peer-to-peer mesh networking signaled
over a Discord lobby (Discord Social SDK for auth/lobby, libjuice for ICE).

No Godot dependency here — see [DiscordP2PMesh-GD](https://github.com/HeatXD/DiscordP2PMesh-GD)
for the Godot GDExtension built on top of this.

## C API

Public header: `include/discord_p2p_mesh.h`. Modeled after GekkoNet's C API: an opaque session
handle, a config struct, and a poll-driven event queue instead of callbacks crossing the C
boundary.

```c
DPMeshConfig config = {0};
config.application_id = 1234567890;
config.stun_server_host = "stun.l.google.com";
config.stun_server_port = 19302;

DPMeshSession *session = dpmesh_create(&config);
dpmesh_login(session);

while (running) {
    dpmesh_update(session); // pumps the Discord SDK + P2P mesh once per frame/tick

    DPMeshEvent event;
    while (dpmesh_poll_event(session, &event)) {
        // handle event.type ...
    }
}

dpmesh_destroy(session);
```

Event payload pointers (strings, byte buffers) are valid until the next `dpmesh_update()` call
on that session — read them before calling update again.

See `examples/console/main.c` for a minimal end-to-end usage without any engine involved.

## Building (Windows, Visual Studio)

1. Vendor the Discord Social SDK per `thirdparty/README.md`.
2. Open `DiscordP2PMesh.slnx` and build. vcpkg manifest mode pulls in `libjuice` automatically.

Output lands in `bin/windows/<Debug|Release>/`: `discordp2pmesh.dll`/`.lib`, plus the
`discord_partner_sdk.dll` and `juice.dll` runtime dependencies copied alongside it.
