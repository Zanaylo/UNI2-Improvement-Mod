# UNI2 Rank Wars (Add-on)

Standalone Lua add-on for **UNI2 Improvement Mod** (`UNI2-IM\Addons\RankWars`).

Zero extra DLLs, zero C++ rebuilds — updates independently from the base Improvement Mod via **UNI2 Mod Loader**.

## Features

- Reads Rank Wars launch links (`create`, `join`, `replay`) from command line and Steam launch query.
- Automates Player Match room creation (2-player capacity and first-to rematch limit) and guest room joining.
- Captures match results and uploads verified `.rep` replays to the Rank Wars API with exponential retry.
- Draws live match score pips directly in the in-battle HUD (`hud.lua`).
- Full in-game ImGui status/scoreboard window, Toast notifications, Steam Rich Presence note, Replay Viewer, and Sandbox test match support.

## Installation

### Via UNI2 Mod Loader (recommended)
1. Open `UNI2ModLoader.exe`.
2. Go to the **Add-ons (Lua / Native)** tab.
3. Paste this repository's GitHub URL and click **Install from GitHub** (or install a release `.zip` / folder).
4. Keep **Rank Wars** checked and click **Play UNI2 (Steam)**.

### Manual
Copy this folder into `<UNI2 game folder>\UNI2-IM\Addons\RankWars\` so that `addon.ini` and `main.lua` sit inside it.
