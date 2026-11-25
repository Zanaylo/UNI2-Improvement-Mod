# Great Lobby — Native Add-on Setup & Migration Guide (Yuko)

## 1. Validation Summary of `great-lobby-teste.zip` (Test Build 16)

| Item | Result | Details |
|------|--------|---------|
| RmlUI Views (`.rml`) | **PASS (18/18)** | All 18 referenced views present (`browser`, `challenge`, `create`, `entry`, `history`, `hud`, `keyboard`, `lobby`, `member`, `notice`, `picker`, `postgame`, `returning`, `settings`, `starting`, `tournament`, `watch`, `watchchat`). All `src` attributes resolve cleanly (the 15 dynamic `*.face` attributes are RmlUI `data-attr-src` bindings). |
| Stylesheet (`lobby.rcss`) | **PASS** | Valid stylesheet; all font and image paths (`bg.png`, `fonts/Barlow*.ttf`) resolve inside `great-lobby/`. |
| Character Faces (`faces/*.png`) | **PASS (28/28)** | Full UNI2 roster portraits present (`akatsuki` .. `zohar`). |
| Fonts (`fonts/*.ttf`) | **PASS (7/7)** | Complete Barlow font family + `OFL.txt`. |
| Binary (`dinput8.dll`, 37 MB) | **FORK — MUST REBUILD AS PLUGIN** | Built from `great-lobby\bin\Release\dinput8.pdb` as a full fork of `UNI2-Improvement-Mod` (contains its own `UNI2_IM_instance_%lu` mutex, MinHook instance, D3D9 wrapper, and ImGui). It cannot be chained alongside another `dinput8.dll`, so it must be compiled as a standalone native add-on DLL (`GreatLobby.dll`) against `AddonApi.h` v2. |

---

## 2. Folder Layout of the `GreatLobby` Add-on

```text
UNI2-IM\Addons\GreatLobby\
├── addon.ini              (Manifest declaring Native = GreatLobby.dll)
├── GreatLobby.dll         (32-bit Win32 native plugin exporting UNI2_AddonInit / UNI2_AddonShutdown)
├── YUKO_SETUP.md          (This guide)
├── great-lobby\           (Validated RmlUI views, lobby.rcss, bg.png, faces/, fonts/)
└── sdk\                   (Ready-to-build Visual Studio Win32 plugin project)
    ├── AddonApi.h         (Host C ABI v2 header)
    ├── GreatLobbyPlugin.cpp
    └── GreatLobbyPlugin.vcxproj
```

---

## 3. Mapping Great Lobby Internals to `Uni2AddonHost` (ABI v2)

`UNI2-Improvement-Mod` now owns **one** D3D9 hook, **one** WndProc hook, **one** MinHook instance, and **one** lifecycle manager. Your `GreatLobby.dll` only needs to link RmlUI + your Great Lobby code and call the host table passed to `UNI2_AddonInit(const Uni2AddonHost* host, const char* addonDir)`:

| What Great Lobby Needs | Host API (`Uni2AddonHost` v2) |
|------------------------|-------------------------------|
| Asset root (`great-lobby/`) | `std::string(addonDir) + "\\great-lobby"` (no hard dependency on `UNI2-IM\great-lobby\`) |
| `IDirect3DDevice9*` for RmlUI DX9 renderer | `host->getD3D9Device()` |
| Game `HWND` | `host->getGameWindow()` |
| RmlUI Render Pass (runs inside `BeginScene`/`EndScene`, under IM toasts/overlay) | `host->registerRenderCallback(OnD3D9Render, user)` |
| Device Lost / Reset (release/recreate RmlUI textures) | `host->registerDeviceCallbacks(OnLost, OnReset, user)` |
| Per-frame tick (Steam callbacks, room/table state, replay watch) | `host->registerFrameCallback(OnFrame, user)` |
| Keyboard / mouse input & hotkeys (`F9`, `ESC`) | `host->registerWndProcCallback(OnWndProc, user)` |
| Block game input / notify IM when lobby UI is open | `host->setOverlayBlocksInput(bool)` |
| XInput pad gate (`XInputGetState`) & Steam callback filter | `host->createHook(...)` / `host->createApiHook(...)` / `host->setHookEnabled(...)` (uses IM's shared MinHook — zero conflicts!) |
| Memory RVA / Sig lookup / safe R/W | `host->rvaToAddress(rva)`, `host->signatureAddress(rva)`, `host->readMemory(...)`, `host->writeMemory(...)` |
| Scene, Match, Online, Spectating, Chara, Local Side | `host->getScene()`, `host->getSceneFrames()`, `host->isInMatch()`, `host->isOnline()`, `host->isSpectating()`, `host->getLocalSide()`, `host->getLoadedCharacter(side)`, `host->getCharacterName(id)` |
| Match start / end & replay capture | `host->registerGameEvents(OnStarted, OnEnded, user)`, `host->getReplayFolder(...)`, `host->saveReplayRecord(...)` |
| Steam flat APIs (`SteamAPI_ISteamMatchmaking_*`, etc.) | Keep resolving from `GetModuleHandleA("steam_api.dll")` directly + `host->getLocalSteamId()` & `host->setRichPresenceNote(...)` |
| Settings persistence (`UNI2-IM` INI) | `host->configGetString`, `host->configGetInt`, `host->configSetString`, `host->configSetInt` |
| IM Overlay launcher button & notifications | `host->addMainMenuButton(...)`, `host->showToast(...)`, `host->notify(...)`, `host->log(...)` |

---

## 4. How to Port & Build `GreatLobby.dll`

1. Open `addons/GreatLobby/sdk/GreatLobbyPlugin.vcxproj` in Visual Studio (Platform: **Win32**, Configuration: **Release**).
2. Add your Great Lobby source files (`src/GreatLobby/*.cpp`) and RmlUI static libs/includes to the project.
3. Replace direct calls to UNI2-IM's internal `HookManager`, `WindowManager`, `DeviceHooks`, `Settings`, and `ReplayFiles` with the `Uni2AddonHost* g_host` table in `GreatLobbyPlugin.cpp`.
4. Point RmlUI's asset root to `g_assetsDir` (`<addonDir>\great-lobby`).
5. Build `Release|Win32` -> outputs `addons/GreatLobby/GreatLobby.dll`.
6. Zip the `addons/GreatLobby/` folder (excluding `sdk/`) and distribute on GitHub Releases or Google Drive — players install it in one click through **UNI2 Mod Loader**.
