# Third-party SDKs

## Discord Social SDK

Not included in this repo — Discord's terms don't allow redistributing the SDK itself.

1. Download it from https://discord.com/developers/docs/discord-social-sdk (requires a Discord developer account).
2. Extract it so the layout looks like this:

```
thirdparty/discord_social_sdk/
  include/
    discordpp.h
    cdiscord.h
  lib/
    release/discord_partner_sdk.lib
    debug/discord_partner_sdk.lib
  bin/
    release/discord_partner_sdk.dll
    debug/discord_partner_sdk.dll
```

Only the Windows x86_64 pieces are needed for this project (`include/`, and the `release`/`debug` folders under `bin/` and `lib/` — skip `arm64`, the mobile/macOS frameworks, `.aar`/`.so`/`.dylib` files, and the `discord_krisp*` files unless voice features are added later).

3. Build the `DiscordP2PMesh` project as usual — the vcxproj references `thirdparty/discord_social_sdk/include` and `thirdparty/discord_social_sdk/lib/<config>`, and a post-build step copies the matching `discord_partner_sdk.dll` next to the built library in `bin/windows/<config>/`.
