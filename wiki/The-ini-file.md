# The ini file

`UNI2_IM.ini` sits in the `UNI2-IM` folder next to the DLL. The mod fills it in on every run. A
missing key or section is added with its default, and a key you edited is left alone. Delete the
file to start over. The file always lists every setting your build knows, and keys from a new
version show up on the next launch.

## `[Mod]`

| Key | Default | What it does |
|---|---|---|
| `DinputDllWrapper` | empty | Full path to another `dinput8.dll` to chain-load. Empty uses the system one. |
| `CheckForUpdates` | `1` | Checks GitHub once, in the background, for a newer release. Nothing is downloaded until you press **Update now**. |
| `SettingsRevision` | `2` | Which release's defaults this file was last updated to. The mod uses it to fix a setting whose old default turned out to be unsafe. Do not edit it. |

## `[Keybinds]`

| Key | Default | What it does |
|---|---|---|
| `ToggleOverlay` | `F1` | Opens and closes the main window. |
| `ToggleHitboxOverlay` | `F2` | Hitbox viewer. |
| `ToggleFrameMeter` | `F3` | Frame meter. |
| `FreezeFrame` | `F5` | Pause and resume. |
| `StepForward` | `F6` | One frame forward. Hold to repeat. |
| `NextPalette` | `F8` | Next palette on the character you are playing. After the last one it goes back to the game's own colours. |
| `PreviousPalette` | `F7` | The same, backwards. |
| `FunctionKey` | empty | A key you hold together with another one, like a shortcut. To use it, start the bind with `Fn+` (for example `Fn+F8`). While it is held, binds without the prefix are ignored, so one key can do two things. |

## `[PadKeybinds]`

A pad bind is always the function button **plus** one other button. Pad binds use XInput, with
XInput's names: `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `L3`, `R3`, `Start`, `Back`, `Guide`,
`DPad Up`, `DPad Down`, `DPad Left`, `DPad Right`. An empty value means unbound.

| Key | Default | What it does |
|---|---|---|
| `FunctionButton` | `Back` | The button you hold for every pad bind. |
| `ToggleOverlay` | empty | Opens and closes the main window. |
| `ToggleHitboxOverlay` | empty | Hitbox viewer. |
| `ToggleFrameMeter` | empty | Frame meter. |
| `FreezeFrame` | empty | Pause and resume. |
| `StepForward` | empty | One frame forward. Hold to repeat. |
| `NextPalette` | empty | Next palette on the character you are playing. |
| `PreviousPalette` | empty | The same, backwards. |

## `[Input]`

| Key | Default | What it does |
|---|---|---|
| `KeyboardSeat` | `0` | Which player the keyboard controls. 0 leaves the game alone, 1 puts your keys on 1P, 2 on 2P. |
| `KeyboardSeatRouteSides` | `1` | Also writes both sides' controller slots every frame of a local match. |

## `[Training]`

| Key | Default | What it does |
|---|---|---|
| `FreezeMode` | `0` | 0 tick stop, 1 hitstun stop. Tick stop freezes everything. Hitstun stop keeps menus working but distorts effects. |
| `AutoPauseOnAttack` | `0` | Add the numbers together: 1 watch P1, 2 watch P2, 4 on attacks, 8 on armoured moves. 0 is off. |
| `AutoPauseComboStops` | `3` | Hit counts to stop at, comma separated. `3,20` stops on the third hit and on the twentieth. |
| `AutoPauseBlockStops` | `3` | The same, for blocked hits. |
| `ResumeDelayFrames` | `60` | Countdown before the game resumes after an auto pause. |
| `StepRepeatDelayMs` | `250` | How long you hold the next-frame key before it starts repeating. |
| `StepRepeatIntervalMs` | `90` | Time between repeated steps. |
| `RecordFrameCounterRva` | `0` | Advanced. Memory address of the recorder's frame counter. 0 turns it off. |

## `[FrameMeter]`

| Key | Default | What it does |
|---|---|---|
| `PlaceAutomatically` | `0` | Keeps the meter centred near the bottom of the screen at any resolution. The position below is ignored. |
| `PositionX` / `PositionY` | `-1` | Top-left corner in pixels. `-1` means it was never placed: the meter takes the automatic spot once, saves it here, and you can drag it from there. |
| `Scale` | `1.5` | Size of the meter. |
| `BandCounts` | `1` | Shows the length of each finished band inside the bar. |
| `LineTotals` | `1` | Shows blockstun, hitstun and the gap for the exchange, plus the super flash inside the move, on their own line. |
| `AttributeRow` | `1` | The thin row under each bar that names every active invincibility. |
| `Opacity` | `100` | How solid the meter looks, in percent. |
| `MouseDrag` | `1` | Lets you drag the meter with the mouse. |

## `[Palette]`

| Key | Default | What it does |
|---|---|---|
| `ShowOnlinePalettes` | `1` | Controls the other player's side. On, their palette is applied when it arrives. Off, it is ignored and their side keeps the game's colours. Yours is sent either way. |
| `Creator` | empty | The author name saved into your palettes. The overlay fills it in as you type. |
| `CompanionCharacters` | `15` | Characters whose companion is drawn before the fighters, by the game's own numbering. Chaos is 15. Comma separated. |
| `OwnersFromDraws` | `1` | Finds which character owns a texture from the actual draw calls instead of guessing from bind order. Mirror matches need this on. |
| `IdentifyByColours` | `0` | Lets the colour comparison decide which side is which. Leave it off: every time it was tested it got the sides backwards. |
| `PaintOutOfMatch` | `0` | Also applies chosen palettes outside a match, such as the character select portrait and the lobby avatar. |
| `PaintEffectRows` | `1` | Does nothing now. It stays so an older ini still loads. |
| `ShowLegacyTab` | `0` | Shows the tab of the first palette system. |
| `GroupByPart` | `1` | Groups palette entries like the game's own colour screen does (hair, skin and so on). |
| `FlashEntry` | `1` | When you pick an entry, everything else darkens and that entry blinks on the character. |
| `FilterJunk` | `1` | Hides entries that are not real colours: the black padding, the green filler in unused slots, and repeats of an entry above. |

## `[Netplay]`

| Key | Default | What it does |
|---|---|---|
| `RoomRosterFix` | `0` | The game only removes a room member when they `Left`, so `Disconnected`, `Kicked` and `Banned` leave a ghost behind. On, those are handled like `Left`. It changes how the game handles its own room, so it is off by default and never acts while a match is connected. |
| `RepublishPingLocation` | `0` | Sends your Steam ping location to the room every 30 s. The game only sends it once, when you join, which is why rejoining "resets" the ping. It writes a key the game owns, so it is off by default and never runs while a match is connected. |
| `NetLog` | `1` | Writes `Logs\UNI2_IM_NET_*.log`: every Steam event the game receives, every packet the mod sends or receives, and one line per second during a match with frame times, rollbacks, ping and the send queue. It is written on its own thread. Send this file with any connection report. |
| `CaptureGgpoLog` | `0` | Adds GGPO's own log lines (syncing, lost packets, disconnects) to the network log. The game formats them and throws them away; this hooks those two log functions and nothing else. For a detailed report only. |
| `SharePalettes` | `1` | Sends your palette to the other player over the mod's own Steam channel, only when they run the mod, only once the match connection is fully up and not busy, and never more than 32 KB in a match. An opponent without the mod receives nothing from the mod at all. |

## `[Video]`

| Key | Default | What it does |
|---|---|---|
| `TimerResolution` | `1` | Keeps Windows' 1 ms timer and asks for it again when the window gets focus back. The game only asks once at startup, and Windows takes it away in the background. That is what makes the game worse after an alt-tab. |
| `PowerThrottlingOptOut` | `1` | Opts the game out of EcoQoS and of the background timer limit. The other half of the same fix. |
| `PumpWait` | `0` | Waits on the frame thread's message instead of the clock, and puts the engine's other short sleeps on a high resolution timer. No CPU cost, no game code patched. |
| `PumpWaitAllInput` | `0` | Wakes that wait on every message, not only the handshake. Lowers window message latency, but uses more CPU the more you move the mouse. |
| `DisplayTuning` | `1` | Lets the mod choose the fullscreen display settings below. Off keeps exactly what the game asked for. |
| `FullscreenRefreshHz` | `0` | 0 keeps the desktop's mode. If the game's vsync is on and the rate is not a multiple of 60, 0 picks the highest listed multiple of 60 at or below the desktop rate. Exclusive fullscreen only. |
| `ExtraBackBuffer` | `0` | Adds a second back buffer. Only helps in exclusive fullscreen with the game's vsync on, and adds up to one frame of input latency. Ignored in windowed mode and with vsync off. |
| `FlatStage` | `0` | Replaces the stage with a flat colour, for keying a capture. |
| `FlatStageColour` | `65280` | That colour, as `0xRRGGBB` written in decimal. |
| `ScreenShake` | `100` | How much of the game's screen shake to keep, 0 to 100. Every shake (a move, Wald's walk, a cutscene) is scaled by this, so it keeps its shape and length. 0 cancels the shake. Also a slider on the Config tab. |

## `[Music]`

| Key | Default | What it does |
|---|---|---|
| `KeepMenuMusic` | `1` | Keeps the menu music playing when you go into Options, Customize and Gallery. Without it, the game restarts the track every time you come back. 0 is the game's normal behaviour. Also a checkbox in the Music section. |

## `[Graphics]`

Set `PotatoMode` and leave the rest alone. It is a preset for the keys below, and turning it off
restores them. Change the other keys only if you want to push one further than the preset does.

| Key | Default | What it does |
|---|---|---|
| `PotatoMode` | `0` | 0 off, 1 balanced, 2 potato, 3 extreme potato. |
| `DisableBackBufferAA` | `0` | Asks for a back buffer with no multisampling. The scene is never antialiased anyway, so the samples do nothing. |
| `DisableCharacterFilter` | `0` | Keeps the game's own Character Visual Improvements off. It costs nine palette lookups per pixel for a one pixel blur. |
| `PresentWidth` | `0` | The width the finished frame is drawn at before it is stretched to your window. 0 keeps the game's own Display option. |
| `PresentHeight` | `0` | The height, same rule. Set both, or neither does anything. Windowed and borderless only. |
| `PotatoHeight` | `360` | The size the Potato level uses, as the height of a 16:9 picture: 480, 360, 240 or 144. |
| `Supersample` | `0` | The Improvements tab: 0 off, 1 draws at 1440p, 2 at 4K, and Direct3D fits that to your window. Ignored while `PotatoMode` is set, because both set the drawing size. |
| `Sharpen` | `0` | Sharpening on the finished frame, 0 to 100. 0 is off, 40-60 is the useful range. Applies right away and works at any drawing size, POTATO MODE included. |
| `SharpenMode` | `0` | Which sharpening method: 0 off, 1 contrast adaptive, 2 FidelityFX RCAS. |
| `UpscaleFilter` | `0` | Which filter scales the scene up to your window, instead of the engine's bilinear: 0 off, 1 bicubic, 2 Lanczos, 3 FidelityFX EASU. Only works when the back buffer is larger than 1280x720. |
| `Bloom` `BloomIntensity` `BloomThreshold` | `0` `40` `75` | Bloom on the finished frame. `Bloom` turns it on. The other two go from 0 to 100. |
| `Look` | `0` | Turns the colour and display pass on. Off, none of the `Look*` values below are used. |
| `AntiAliasing` | `0` | FXAA on the finished frame: 0 off, 1 low, 2 medium, 3 high, 4 ultra. Multisampling does not work in this game, so supersampling and this filter are the only antialiasing options. |
| `LookBrightness` `LookContrast` `LookSaturation` `LookVibrance` `LookTemperature` | `0` | The colour pass, -100 to 100 each. The pass does not run while all of them are at 0. |
| `LookGamma` | `100` | Gamma as a percentage of 1.0. |
| `LookVignette` `LookScanlines` | `0` | 0 to 100 each. |
| `LookDither` | `0` | Adds a pixel of noise to hide the banding gradients get on an 8 bit back buffer. |
| `ShaderPack` | empty | The user shader that runs last, by file name, from `UNI2-IM/Shaders` (`.hlsl`, `.ps`, `.fx`, `.slang`, `.glsl`, `.frag`, `.fsh`). Needs `d3dcompiler_47.dll`, which comes with Windows and with Proton. |
| `SimpleStage` | `0` | Draws the empty stage instead of the full one. Not part of any POTATO MODE level on purpose. |

`PresentWidth` and `PresentHeight` are **derived** from `PotatoMode` + `PotatoHeight` +
`Supersample`, and rewritten whenever any of those change. They are what Direct3D receives.

## `[Overlay]`

| Key | Default | What it does |
|---|---|---|
| `UiScale` | `1.0` | Overlay scale. 1.0 is native. |
| `FontPath` | empty | A `.ttf` for the overlay. Empty picks the first scalable font on the system (Segoe UI on Windows, usually DejaVu Sans under Proton). |
| `FontSize` | `16.0` | Font size in pixels before scaling. |
| `DpiAware` | `0` | Tells Windows the game handles its own scaling. Off, with display scale above 100%, Windows draws the window small and stretches it, which blurs everything. Needs a restart. The Config tab tells you whether it worked. |
| `Notifications` | `1` | The message that slides across the top when the mod loads. 0 hides it. |
| `BlockGameMouse` | `0` | Hides the mouse from the game, so clicking the overlay cannot affect it. |
| `DrawWhileGamePaused` | `0` | Keeps the hitbox viewer and frame meter visible while the game's pause menu is open. Off, both hide. |

## `[Debug]`

| Key | Default | What it does |
|---|---|---|
| `MemoryDebug` | `0` | Loads the Memory debug window, opened with Ctrl+F1. |
| `Profiler` | `0` | Frame interval and per-section timing, shown in the Performance window's Metrics tab. |
| `MeterTrace` | `0` | The frame meter's diagnostic capture and its CSV. |

`Logging = 1` turns logging on. Nothing is written without it. If someone reports the mod does
nothing, ask for the log first: it shows startup, every hook the mod installed and where, and
anything that crashed.

## `[Compat]`

| Key | Default | What it does |
|---|---|---|
| `WineSafeMode` | `-1` | `-1` is automatic: on under Wine/Proton, off on Windows. `1` forces it on, `0` forces it off. On, the mod does not touch presentation or scheduling: no fullscreen refresh change, no power throttling opt-out, no `Sleep` substitution. On Linux, set `0` to test whether one of those three is causing a problem. |

## `[Extras]` and `[Stages]`

Written by the [Stages](Stages) window. Do not edit them by hand.

| Key | What it holds |
|---|---|
| `[Extras] UnlockedStages` | The hidden stage numbers you ticked, comma separated. |
| `[Stages] StageNN` | One ported stage: source game, its folder there, and the name shown in the picker. |
