# DiscordP2PMesh
### C/C++ Peer To Peer Mesh Networking SDK built on Discord

DiscordP2PMesh connects players directly to each other, without you running any servers.

Discord does the parts around the connection: players log in with their Discord account, join a lobby by secret, and the offer/answer messages peers need to find each other travel through that lobby as hidden chat messages. [libjuice](https://github.com/paullouisageneau/libjuice) then does the ICE negotiation that opens a direct UDP path between them, punching through NATs along the way.

You drive it through a plain C API. Once two players are connected you get a peer ID and raw byte buffers in both directions, and what you send over them is up to you.

## Project Goals
### Done
- Discord authentication (OAuth device flow, handled by the SDK)
- Lobbies
	- Create or join by secret
	- Lobby chat messages
	- Member join/leave notifications
- Peer to peer connections
	- ICE negotiation over the lobby, no signaling server of your own
	- Configurable STUN/TURN servers
	- Glare resolution when both sides connect at once
- Rich presence updates
- Poll based event system, no callbacks across the C boundary
- Automated builds for Windows, Linux and macOS

### Maybe Later
- Reliability/ordering options on top of the raw UDP path
- Voice support

## Getting Started
### Docs
- Need a Discord application first? See [`docs/discord_application.md`](docs/discord_application.md)
- The API lives in a single header: [`include/discord_p2p_mesh.h`](include/discord_p2p_mesh.h)
- Look at [`examples/console`](examples/console) to see how it all fits together, it exercises every function and event in the API

The shape of it:

```c
DPMeshConfig config = {0};
config.application_id = 1234567890;
config.stun_server_host = "stun.l.google.com";
config.stun_server_port = 19302;

DPMeshSession *session = dpmesh_create(&config);
dpmesh_login(session);

while (running) {
    dpmesh_update(session);

    DPMeshEvent event;
    while (dpmesh_poll_event(session, &event)) {
        // handle event.type
    }
}

dpmesh_destroy(session);
```

Event payload pointers stay valid until the next `dpmesh_update()` call on that session, so read them before you update again. All calls for a session must happen on the same thread that drives `dpmesh_update()`.

## Using it in Godot
There is a GDExtension wrapper over this library at [DiscordP2PMesh-GD](https://github.com/HeatXD/DiscordP2PMesh-GD).

## Building Examples
- The examples are built and ran using Visual Studio through `DiscordP2PMesh.slnx`, which builds the library alongside them
- Windows only for now

## Building DiscordP2PMesh
### Prerequisites
To build DiscordP2PMesh, make sure you have the following installed:

1. **CMake** (version 3.16 or higher)
2. **C++ Compiler** with C++20 support:
   - **GCC** or **Clang** (Linux/macOS)
   - **MSVC** (Visual Studio) for Windows
3. **The Discord Social SDK**, see [thirdparty/README.md](thirdparty/README.md). It's proprietary so it can't be shipped in this repo, you download it once and drop it in place.

libjuice comes along as a submodule and is built and linked statically, so there is no package manager to set up.

### Step-by-Step Instructions

#### 1. Clone the Repository
```sh
git clone --recurse-submodules https://github.com/HeatXD/DiscordP2PMesh.git
cd DiscordP2PMesh
```

#### 2. Add the Discord Social SDK
Follow [thirdparty/README.md](thirdparty/README.md) so the SDK ends up at `thirdparty/discord_social_sdk/`. If you keep it somewhere else, point `DPMESH_DISCORD_SDK_DIR` at it instead.

#### 3. Configure Build Options
DiscordP2PMesh includes a few options to customize the build:

- `BUILD_SHARED_LIBS`: Set to `ON` to build a shared library, or `OFF` for a static one.
- `DPMESH_DISCORD_SDK_DIR`: Path to the Discord Social SDK, defaults to `thirdparty/discord_social_sdk`.

To configure these options, use `cmake` with `-D` flags. For example:

```sh
cmake -S . -B build -DBUILD_SHARED_LIBS=ON
```

#### 4. Build the Project
```sh
cmake --build build
```

On successful completion, the library will be located in the `out` directory inside your build directory, together with the Discord SDK runtime it needs.

On Windows you can also just open `DiscordP2PMesh.slnx` in Visual Studio, which builds the library and the examples without CMake.

### Build Output
- **Library**: `discordp2pmesh`, shared or static depending on `BUILD_SHARED_LIBS`
- **Runtime dependency**: `discord_partner_sdk`, copied next to the library. libjuice is linked in statically, so it doesn't ship separately.

---

## License
DiscordP2PMesh is licensed under the BSD-2-Clause license
[Read about it here](https://opensource.org/license/bsd-2-clause).

The Discord Social SDK is not covered by that license and is not redistributed here, it remains subject to Discord's own developer terms.
