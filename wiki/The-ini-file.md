# The ini file

`UNI2_IM.ini` sits in the `UNI2-IM` folder next to the DLL, and the mod completes it on every run:
a missing key or section is appended with its default, an edited one is left exactly as it is, and
the whole file can be deleted to start over. So the file always lists every setting this build
understands, and keys added by a new version turn up in it on the next launch.

## `[Mod]`

| Key | Default | What it does |
|---|---|---|
| `DinputDllWrapper` | empty | Full path to another `dinput8.dll` to chain-load. Empty uses the system one. |
| `CheckForUpdates` | `1` | Asks GitHub once, on a thread of its own, whether a newer release exists. Nothing is downloaded until you press **Update now**. |
| `SettingsRevision` | `2` | Which release's defaults this file was last brought up to. A lower number lets the mod correct a setting whose old default turned out to be unsafe. Never edit it by hand. |

## `[Keybinds]`

| Key | Default | What it does |
|---|---|---|
| `ToggleOverlay` | `F1` | Opens and closes the main window. |
| `ToggleHitboxOverlay` | `F2` | Hitbox viewer. |
| `ToggleFrameMeter` | `F3` | Frame meter. |
| `FreezeFrame` | `F5` | Pause and resume. |
| `StepForward` | `F6` | One frame forward; hold to repeat. |
| `NextPalette` | `F8` | Next palette on the character you are playing; wraps back to the game's own colours. |
| `PreviousPalette` | `F7` | The same, backwards. |
| `FunctionKey` | empty | Held with another key, the way a fighting game does shortcuts. A bind asks for it by carrying an `Fn+` prefix - `Fn+F8` - and a bind without the prefix is ignored while it is held, so one key can serve both. |

## `[PadKeybinds]`

Pad binds are always the function button **plus** one other, and they read XInput. Names are
XInput's: `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `L3`, `R3`, `Start`, `Back`, `Guide`,
`DPad Up`, `DPad Down`, `DPad Left`, `DPad Right`. An empty value is unbound.

| Key | Default | What it does |
|---|---|---|
| `FunctionButton` | `Back` | The button every pad bind is held with. |
| `ToggleOverlay` | empty | Opens and closes the main window. |
| `ToggleHitboxOverlay` | empty | Hitbox viewer. |
| `ToggleFrameMeter` | empty | Frame meter. |
| `FreezeFrame` | empty | Pause and resume. |
| `StepForward` | empty | One frame forward; hold to repeat. |
| `NextPalette` | empty | Next palette on the character you are playing. |
| `PreviousPalette` | empty | The same, backwards. |

## `[Input]`

| Key | Default | What it does |
|---|---|---|
| `KeyboardSeat` | `0` | Which player number the keyboard is: 0 leaves the game alone, 1 puts your own keys on 1P, 2 on 2P. |
| `KeyboardSeatRouteSides` | `1` | Whether the seat also writes both sides' controller slots every frame of a local match. |

## `[Training]`

| Key | Default | What it does |
|---|---|---|
| `FreezeMode` | `0` | 0 tick stop, 1 hitstun stop. Tick stop freezes everything; hitstun stop keeps menus live but distorts effects. |
| `AutoPauseOnAttack` | `0` | Bit field: 1 watch P1, 2 watch P2, 4 on attacks, 8 on armoured moves. 0 is off. |
| `AutoPauseComboStops` | `3` | Hit counts to stop at, comma separated. `3,20` stops on the third hit and the twentieth. |
| `AutoPauseBlockStops` | `3` | The same, for blocked hits. |
| `ResumeDelayFrames` | `60` | Countdown before the game resumes after an auto pause. |
| `StepRepeatDelayMs` | `250` | How long the next-frame key must be held before it repeats. |
| `StepRepeatIntervalMs` | `90` | How long between repeated steps. |
| `RecordFrameCounterRva` | `0` | Advanced. RVA of the recorder's frame counter; 0 disables it. |

## `[FrameMeter]`

| Key | Default | What it does |
|---|---|---|
| `PlaceAutomatically` | `0` | Keeps the meter centred near the bottom of whatever resolution the game runs at, ignoring the position below. |
| `PositionX` / `PositionY` | `-1` | Top-left corner in pixels. `-1` means it has never been placed: the meter takes the automatic spot once, writes it here, and is draggable from there. |
| `Scale` | `1.5` | Size of the meter. |
| `BandCounts` | `1` | Print the length of every finished band inside the bar. |
| `LineTotals` | `1` | Blockstun, hitstun and the gap for the exchange, plus the super flash inside the move, on its own line. |
| `AttributeRow` | `1` | The thin row under each bar naming every invincibility in force. |
| `Opacity` | `100` | How solid the meter is drawn, as a percentage. |
| `MouseDrag` | `1` | Whether a click that lands on the meter drags it. |

## `[Palette]`

| Key | Default | What it does |
|---|---|---|
| `ShowOnlinePalettes` | `1` | The other player's side, in one decision. On, their palette is read as it arrives and worn. Off, their packets are dropped and their side is left as the game gives it. Yours is sent either way. |
| `Creator` | empty | The author name written into palettes you save. The overlay fills this in as you type it. |
| `CompanionCharacters` | `15` | Characters whose companion draws before the fighters do, by the game's own numbering. Chaos is 15. Comma separated. |
| `OwnersFromDraws` | `1` | Take texture owners from the renderer's own draw calls instead of the bind-order guess. A mirror match needs this on. |
| `IdentifyByColours` | `0` | Let the colour comparison name a side. Off, and off for a measured reason: it has come out backwards every time it was tried. |
| `PaintOutOfMatch` | `0` | Paint chosen palettes outside a match too - character select portrait, lobby avatar. |
| `PaintEffectRows` | `1` | Reserved; it no longer does anything. Left so an older ini still loads. |
| `ShowLegacyTab` | `0` | Shows the first palette system's tab. Kept for its machinery only. |
| `GroupByPart` | `1` | Group palette entries the way the game's own colour screen does - hair, skin, and so on. |
| `FlashEntry` | `1` | Picking an entry darkens everything else and blinks that entry on the character. |
| `FilterJunk` | `1` | Hide the entries that are not really colours: the black padding, the green the unused slots are filled with, and anything repeating an entry above it. |

## `[Netplay]`

| Key | Default | What it does |
|---|---|---|
| `SafeOnline` | `1` | While a netplay session is up the mod writes nothing anybody else receives and calls nothing the netcode owns. Everything below still runs in a room. Off, each switch decides for itself again - which is what the mid-match disconnects were traced to. |
| `RoomRosterFix` | `1` | The game removes a room member only on an exact `Left`, so `Disconnected`, `Kicked` and `Banned` leave a ghost behind. On, those are routed to the handler the game uses for `Left`. `SafeOnline` holds it back once a session is up, because a blip Steam reports as `Disconnected` would otherwise take the opponent out of the room mid-match. |
| `RepublishPingLocation` | `1` | Republishes your Steam ping location into the room every 30 s. The game publishes it once, on join, which is why rejoining "resets" the ping. Held back during a session. |
| `Diagnostics` | `0` | Asks GGPO for ping and frame advantage by calling a method on the game's own backend from the render thread - the netcode thread's object. Off by default for that reason, and throttled to once every twenty frames when on. The rollback and frame counters work either way; those are plain reads. |
| `SharePalettes` | `1` | Sends your palette to the other player over the mod's own Steam channel. It shares a connection with the rollback traffic, so it is sent once when the opponent is not known to be running the mod and three times when they are. |
| `UnloadPatchOnline` | `1` | A patch is battle data the other side does not have. On, going online unloads it and says so, which turns a desync into a restart. Off, the mod only warns. |

## `[Video]`

| Key | Default | What it does |
|---|---|---|
| `TimerResolution` | `1` | Hold Windows' 1 ms timer and ask again when the window regains focus. The game asks once at startup and never again, and Windows takes it back in the background - which is what an alt-tab leaves behind. |
| `PowerThrottlingOptOut` | `1` | Opt the process out of EcoQoS and of the background clamp on timer resolution. The other half of the same fix. |
| `PumpWait` | `0` | Wait on the frame thread's message instead of on the clock, and put the engine's other short sleeps on a high resolution timer. No CPU cost, no engine code patched. |
| `PumpWaitAllInput` | `0` | Wake that wait on every message rather than only on the handshake. Shortens window message latency and costs CPU in proportion to how much the mouse moves. |
| `DisplayTuning` | `1` | Let the mod choose the fullscreen display parameters below. Off leaves exactly what the game asked for. |
| `FullscreenRefreshHz` | `0` | 0 leaves the desktop's own mode alone. With the game's vsync on and a rate that is not a multiple of 60, 0 picks the highest listed multiple of 60 at or below the desktop rate. Exclusive fullscreen only. |
| `ExtraBackBuffer` | `0` | A second back buffer. Only helps in exclusive fullscreen with the game's vsync on, and costs up to a frame of input latency. Ignored windowed and with vsync off. |
| `FlatStage` | `0` | Replace the stage with a flat colour, for keying a capture. |
| `FlatStageColour` | `65280` | That colour, as `0xRRGGBB` in decimal. |
| `ScreenShake` | `100` | How much of the game's own screen shake to keep, 0 to 100. Every shake - a move, Wald's walk, a cutscene - is one call asking the camera to quake, and the slot it fills carries a percentage the engine multiplies the amplitude by, so this rescales that and the shake keeps its shape and its length. 0 answers the call with a duration of zero, which is how the engine cancels one itself. Also a slider on the Config tab. |

## `[Music]`

| Key | Default | What it does |
|---|---|---|
| `KeepMenuMusic` | `1` | Keep the menu music playing across Options, Customize and Gallery. The game's own menu BGM chooser rebuilds the track from the start unless it is still running with the same id when you come back, and those screens pause it on the way in, so it always restarts. On, the mod holds the paused track for the chooser and resumes it. 0 is the game's own behaviour. Also a checkbox in the Music section. |

## `[Graphics]`

Set `PotatoMode` and leave the rest alone: it is a preset over the keys under it, and turning it off
puts them back. They are here for taking one of them further than a preset does.

| Key | Default | What it does |
|---|---|---|
| `PotatoMode` | `0` | 0 off, 1 balanced, 2 potato, 3 extreme potato. |
| `DisableBackBufferAA` | `0` | Ask for a back buffer with no multisampling. The scene is never antialiased anyway, so the samples buy nothing. |
| `DisableCharacterFilter` | `0` | Hold the game's own Character Visual Improvements off: nine palette lookups a pixel for a one pixel blur. |
| `PresentWidth` | `0` | The width the finished frame is drawn at before it is stretched to your window. 0 leaves the game's own Display option alone. |
| `PresentHeight` | `0` | The height, same rule. Both have to be set for either to do anything. Windowed and borderless only. |
| `PotatoHeight` | `360` | Which size the Potato level uses, as the height of a 16:9 picture: 480, 360, 240 or 144. |
| `Supersample` | `0` | The Improvements tab: 0 off, 1 draws at 1440p, 2 at 4K, and Direct3D fits that to your window. Ignored while `PotatoMode` is set - the two settle the same size from opposite ends. |
| `Sharpen` | `0` | Sharpening over the finished frame, 0 to 100. 0 is off, 40-60 is the useful range. Immediate; works at any drawing size, POTATO MODE included. |
| `SharpenMode` | `0` | Which kernel that uses: 0 off, 1 contrast adaptive, 2 FidelityFX RCAS. |
| `UpscaleFilter` | `0` | Which kernel magnifies the scene on its way to your window, in place of the engine's bilinear: 0 off, 1 bicubic, 2 Lanczos, 3 FidelityFX EASU. Only does anything where the back buffer is larger than 1280x720. |
| `Bloom` `BloomIntensity` `BloomThreshold` | `0` `40` `75` | Bloom over the finished frame. `Bloom` is the switch; the other two are 0 to 100. |
| `Look` | `0` | Whether the colour and display pass runs at all. Off, none of the `Look*` values below is read. |
| `AntiAliasing` | `0` | FXAA over the finished frame: 0 off, 1 low, 2 medium, 3 high, 4 ultra. Multisampling cannot reach this game, so supersampling and this filter are the two things that can. |
| `LookBrightness` `LookContrast` `LookSaturation` `LookVibrance` `LookTemperature` | `0` | The colour pass, -100 to 100 each. The pass does not run at all while every one of them is neutral. |
| `LookGamma` | `100` | Gamma as a percentage of 1.0. |
| `LookVignette` `LookScanlines` | `0` | 0 to 100 each. |
| `LookDither` | `0` | A pixel of noise under the banding a gradient picks up on an 8 bit back buffer. |
| `ShaderPack` | empty | The user shader that runs last in the chain, by file name, out of `UNI2-IM/Shaders` (`.hlsl`, `.ps`, `.fx`, `.slang`, `.glsl`, `.frag`, `.fsh`). Needs `d3dcompiler_47.dll`, which ships with Windows and with Proton. |

`PresentWidth` and `PresentHeight` are **derived** from `PotatoMode` + `PotatoHeight` + `Supersample`
and rewritten whenever any of those change; they are what actually reaches Direct3D.
| `SimpleStage` | `0` | Draw the empty stage instead of the built one. Deliberately not part of any POTATO MODE level. |

## `[Overlay]`

| Key | Default | What it does |
|---|---|---|
| `UiScale` | `1.0` | Overlay scale. 1.0 is native. |
| `FontPath` | empty | A `.ttf` for the overlay. Empty picks the first scalable face the system has - Segoe UI on Windows, usually DejaVu Sans under Proton. |
| `FontSize` | `16.0` | Its size in pixels before scaling. |
| `DpiAware` | `0` | Tell Windows the game handles its own scaling. Off, a display scale above 100% makes Windows render the window small and stretch it, which is a second blur over everything. Has to be set before the game makes its window, so it needs a restart, and the Config tab reports whether it took. |
| `Notifications` | `1` | The line that slides across the top when the mod loads. 0 silences it. |
| `BlockGameMouse` | `0` | Stop the game seeing the mouse at all, so clicking the overlay cannot disturb it. |
| `DrawWhileGamePaused` | `0` | Keeps the hitbox viewer and the frame meter up while the game's own pause menu is open. Off, both hide with the battle tick. |

## `[Debug]`

| Key | Default | What it does |
|---|---|---|
| `MemoryDebug` | `0` | Loads the Memory debug window, opened with Ctrl+F1. |
| `Profiler` | `0` | Frame interval and per-section timing, shown in the Performance window's Metrics tab. |
| `MeterTrace` | `0` | The frame meter's diagnostic capture and its CSV. |

`Logging = 1` is what turns logging on, and nothing is written without it. It is the first thing to
ask for when someone reports that the mod does nothing: the log records the startup trail, every
hook the mod installed and where, and anything that faulted.

## `[Compat]`

| Key | Default | What it does |
|---|---|---|
| `WineSafeMode` | `-1` | `-1` automatic - on under Wine/Proton, off on Windows. `1` forces it on, `0` forces it off. On, the mod leaves the host's presentation and scheduling alone: no fullscreen refresh rewriting, no power throttling opt-out, no `Sleep` substitution. Set `0` on Linux to find out whether one of those three is what is misbehaving. |
