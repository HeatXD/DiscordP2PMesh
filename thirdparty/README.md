# Third-party SDKs

## Discord Social SDK

Not included in this repo, Discord's terms don't allow redistributing the SDK itself.

1. Download it from https://discord.com/developers/docs/discord-social-sdk (requires a Discord developer account).
2. Extract it so the layout looks like this:

```
thirdparty/discord_social_sdk/
  include/
    discordpp.h
    cdiscord.h
  lib/
    release/
    debug/
  bin/
    release/
    debug/
```

Take the pieces matching the platform you're building for. On Windows that's `discord_partner_sdk.lib` under `lib/` and `discord_partner_sdk.dll` under `bin/`, on Linux and macOS it's the `.so` and `.dylib` equivalents. You can skip the mobile frameworks, the `.aar` files and the `discord_krisp*` files unless voice features are added later.

3. Build as usual. Both build systems pick the SDK up from here, and copy the runtime library next to the built library so it can be loaded at runtime.

If you keep the SDK somewhere else, point CMake at it with `-DDPMESH_DISCORD_SDK_DIR=/path/to/discord_social_sdk`.
