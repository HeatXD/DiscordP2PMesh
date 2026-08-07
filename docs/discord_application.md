# Setting Up a Discord Application

DiscordP2PMesh authenticates through the Discord Social SDK, which authorizes against a Discord application rather than against your own backend. You need one before `dpmesh_login()` will do anything.

## 1. Create the application

- Go to the [Discord Developer Portal](https://discord.com/developers/applications) and create a new application.
- Your **Application ID** is on the General Information page. That's the `application_id` field in `DPMeshConfig` (or the `application_id` property on the `DiscordP2PMesh` node in Godot).

## 2. Set it up as a public client

DiscordP2PMesh authorizes straight from the game process, there's no backend server to hold a client secret. That means the application has to be configured as a client whose credentials live entirely in the app you ship, what Discord calls a "public client":

- On the OAuth2 tab, add `http://127.0.0.1/callback` as a redirect URL.
- Enable the **Public Client** toggle, also on the OAuth2 tab.

Without both of those, `dpmesh_login()`'s authorization step will fail.

## 3. Rate limits before approval

New applications can use lobbies and rich presence right away, but lobby create/join, update, and linking calls are rate-limited (a few hundred requests every couple of hours) until you submit the app for Discord's Social SDK approval. That's fine for development, apply for approval before shipping to players.
