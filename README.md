# UNI2 Improvement Mod

Training and quality-of-life mod for **UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`).

It loads as a `dinput8.dll` proxy and draws a Dear ImGui overlay inside the game's Direct3D 9
renderer. Inspired by, and architecturally indebted to,
[BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod).

Donwload the .dll from the [Release](https://github.com/Zanaylo/UNI2-Improvement-Mod/releases)

**[English](#english) · [Português](#português) · [日本語](#日本語)**

---

## A note on online play

**Nothing in this mod is built to give anyone an advantage online, and nothing in it does.**

Every training tool that can alter what the simulation does - frame stepping, freezing, driving a
character by hand, the dummy scripts - is hard-gated to offline modes. The gate is the game's own
peer-to-peer traffic: while the game has sent a packet to an opponent in the last three seconds,
those tools refuse to run. The game uses GGPO rollback netcode, and anything that touches simulation
state during a match desynchronises it.

What does run online is cosmetic and read-only: the custom palettes, which travel beside the game
over Steam rather than through the netcode and cannot affect the match, and the performance options,
which only change how the frame reaches your monitor.

If you find something here that gives an edge in a real match, that is a bug. Report it.

---

# English

## Installing

Copy `dinput8.dll` next to `uni2.exe`:

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

Press **F1** in game for the overlay. To uninstall, delete `dinput8.dll`.

`UNI2_IM.ini` is written with defaults the first time the mod runs, in the `UNI2-IM` folder next to
the DLL. It repairs itself from then on: every run, any key or whole section the file is missing is
appended with its default, and nothing you have edited is touched. A new version that adds settings
therefore adds them to your existing file, and a file you cut down to two lines by hand is filled
back in. Delete it to go back to defaults. Every key is documented under
[The ini file](#the-ini-file).

The frame meter draws with the game's own panel art and font. The mod lifts those files out of the
game's `d` archive into `UNI2-IM\Assets` on first run, so there is nothing to extract by hand and no
game data in the download. Delete the folder and it is rebuilt; if the archive cannot be read the
meter still works, drawn in flat colours.

To chain-load another `dinput8.dll` wrapper, put its full path in `[Mod] DinputDllWrapper`.

### Linux and Steam Deck (Proton)

Copy `dinput8.dll` next to `uni2.exe` exactly as on Windows, then do the one step Windows does not
need - tell Wine to load it:

1. In your Steam library, right click **UNDER NIGHT IN-BIRTH II Sys:Celes** and open **Properties**.
2. Under **General**, in **Launch options**, put this line exactly as written:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

3. Start the game and press **F1**.

That is the whole of it. Nothing is renamed and nothing else is copied.

**Why it is needed.** Wine decides which `dinput8.dll` to load from a per-prefix override rather than
from the folder the file is in, so without that line the DLL sits next to `uni2.exe` and is never
loaded at all - that is what "the mod does nothing on Linux" is. `n,b` reads *native first, then
built-in*: the mod's copy loads, and Wine's own `dinput8` still answers everything the mod hands
through to it, which is why the game's own controller input keeps working.

Proton 9 and newer already do this for a mod's own `dinput8.dll`, so there the line changes nothing
and is safe to leave set. Older Proton does not, and will not load the mod without it.

On Linux the mod turns on **compatibility safe mode** by itself: no fullscreen refresh rewriting, no
power throttling opt-out, no `Sleep` substitution. Those three are tuning for the Windows scheduler
and desktop compositor, and on Linux DXVK and the kernel are already doing that job with better
information. Set `[Compat] WineSafeMode = 0` to take them back - and to find out whether one of them
is what is misbehaving, if something is.

If nothing happens at all, look for a `UNI2-IM` folder next to `uni2.exe`. No folder means the DLL
was never loaded, so the problem is the step above rather than the mod. To get a log out of a
machine where it does load, create `UNI2-IM/UNI2_IM.ini` by hand with just these two lines and start
the game once - the mod fills in the rest of the file by itself:

```ini
[Debug]
Logging = 1
```

### RivaTuner Statistics Server, MSI Afterburner and other overlays

Two overlays in one game means two hook engines on the same Direct3D functions, and the usual way
that ends is one of them writing its jump over the other's. RTSS checks that its own jump is still
there and puts it back when it is not, which takes the other engine's hook with it - the mod's
overlay draws for one frame and then never again, or the game crashes at startup.

The mod no longer writes over anybody. Every hook follows the chain of jumps already at the front of
the function and installs itself at the end of it, which is what the RTSS author asks third parties
to do, so both overlays end up in one working chain and load order stops mattering. The same change
is why the Steam overlay, which hooks the same functions, now composes cleanly as well.

If something still goes wrong, set `[Debug] Logging = 1` in `UNI2_IM.ini` and run the game once: the
log in `UNI2-IM/Logs` names the hook and says what happened to it, and the Debug window shows the
same state live. Two RTSS settings fix the rest:

- **Settings / General / Injection properties, "Use Microsoft Detours API hooking".** This switches
  RTSS to a hooking model built for coexisting with other engines.
- **The game's RTSS profile, Application detection level, None.** RTSS then leaves the game alone
  entirely, and its own overlay with it.

## Features

### Overlay

**F1**. Everything lives in one window with three sections - Training, Custom and Config - plus
separate windows for the hitbox viewer, the frame meter legend, Player Control and Performance.

While you are typing into any text field the keyboard belongs to the overlay and the game is handed
nothing, and the same is true while a keybind is being captured. A key still held when a field lets
go is masked until you release it, so nothing comes out as a move.

### Hitbox viewer

**F2**. Draws every box the engine has, for characters **and** projectiles, in the game's own
colours. Decoration is filtered out by the engine's own `_Exist_NoHantei` flag rather than guessed
at, so what you see is what the game reads.

There is no box for a throw, for the D Shield, or for proximity guard - the game does not have one.
A throw's catch region is an ordinary attack box on a frame carrying a throw attribute.

### Frame meter

**F3**. A meter with a band per state, the frame count printed inside each finished band,
the exchange totalled on its own line, and a status row naming every invincibility in force on that
frame. Frame advantage is measured against the game's own advantage field, not inferred from the
animation.

It starts centred near the bottom of the screen and can be dragged with the mouse. It is drawn
straight onto the back buffer and has none of the overlay's hit testing, so a click that lands on it
moves it whatever else you were doing.

### Pause and frame stepping

**F5** pauses, **F6** steps one frame. Holding F6 repeats.

Two freeze modes. *Tick stop* suppresses the game's whole frame and is the default. *Hitstun Stop*
reuses the engine's own hitstop, which keeps menus live but renders effects wrong - that is inherent
to the technique rather than a defect.

Auto pause can stop the game by itself on an attack, on an armoured move, or at a given hit of a
combo, and resume after a countdown.

### Player Control

A window of its own. It shows what both sides are inputting, live, as a numpad stick and four button
lights. You can point your own pad or keyboard at either character, hold a direction or a button on
the dummy, tap one for a few frames, or run a written script per side.

It also measures input lag: the wall clock from a physical press to that character's own input field
changing. The keyboard and every pad are sampled about a thousand times a second on a thread of
their own, rather than read from the game's once-a-frame poll, which would quantise every answer to
16.7 ms and measure nothing. Both of the pad APIs the game uses are covered.

### Keyboard side

*Config → Keyboard.* Pick whether the keyboard plays **1P** or **2P**, and it becomes a player of
its own with the keys you already configured. Built for local play at a tournament, where the two
players share one machine and one of them is on a keyboard.

The game gives the keyboard and the first controller the same player number, so in local versus
they drive the same character. Picking a side here moves the **controller** to the other one; the
keyboard stays exactly as it is, with the keys already configured.

It never writes your key settings, and it never moves the keyboard between the game's two keyboard
players - both of those were tried, and both broke something. If you have a second keyboard player
configured in the game's own options, those keys answer on the controller's side; set *Keyboard
Player Number* to 1 there to switch them off.

*Hold the side during a match* keeps writing both sides' controller slots while a local match runs,
so the side you picked is the side you get. Turn it off to let the game decide who joins where.

Nothing here runs online.

### Palettes

Any colour on either character, applied live. A colour belongs to the character rather than to a
half of the screen, so it stays with them across a crossover, and what each character wears is
remembered and put back on automatically next time.

Palettes are saved as ordinary `.pal` files - the game's own format, which Hantei-kun also writes -
under `UNI2-IM\Palettes\<character>`, with a name, an author and a description. Effect colours are
part of a palette and are saved and loaded with it. The character palette list has a **Default**
entry that puts the game's own colours back.

Online, your palette is sent to the other player beside the game over Steam. **See the other
player's colours** decides their side in one switch: on, their palette is read as it arrives and
worn, so you see what they chose; off, their packets are dropped and their side is left the way the
game gives it. Yours is sent either way, and a player without the mod sees nothing.

**Palette Nativa** is a different feature: the game's own colour customiser, driven from the
overlay. It builds a colour out of the character's stock palettes, one per part, so it cannot be any
colour you like - but the game saves it and **every** opponent sees it, mod or no mod.

### Player Card

Edits the card the game publishes to your opponent: the four plate layers and the plate title. The
title is free text - the three shop words are only the picker's state - so any phrase you write is
saved and reaches the opponent as written.

### BGM selector

Its own window, opened from **Music** in the mod menu.

Every screen with music goes through one entry point in the game, so any of them can be given a
different track: a character's battle theme, the character select, the VS screen, a menu. The game
ships three matchup themes; this is the same idea without the limit.

**Soundpacks** installs a whole game's soundtrack at once and points every screen at it. **Get OST
from French-Bread games** reads one out of a copy you already own - point it at the folder holding
`UNIclr.exe` or `UNIst.exe`, `MBTL.exe`, or `MBAA.exe` - and installs it here with song titles and
loop points. Nothing is downloaded, and no audio ships with the mod. Running it twice on the same
game replaces what it added rather than doubling it. Export and Import move your packs to and from
a single zip, so a friend can have the same set without repeating any of this.

**Browse** lists every track the game can play, searchable and filtered by source, with Play to
start one and hold it - the game gets its music back when you press Stop. The **Randomizer** hands
the game a random track from that list every time it asks for music, and every track has a switch,
so anything you turn off is never picked.

Every track also has a **Volume** of its own, remembered in `UNI2-IM/bgm.ini` and applied the moment
you let go of the slider. It is the engine's own per-slot field, which can only hold a track back:
100% is the level the track was recorded at and there is no way past it. So the way to hear one
quiet track properly is to raise the game's BGM volume until that one is right, then pull the ones
that are now too loud - the battle themes, usually - down here. **Reset volumes** puts every track
back to 100%.

**Rules** is the manual version: play this track for this matchup, for this character, or in place
of this screen. Rules can be exported and imported as well, and importing adds to your list instead
of replacing it.

**Add music** takes your own. Press **Import music** and pick an MP3, OGG or WAV, or drop files into
`UNI2-IM/Music` yourself - loose or a folder per pack - and they turn up in Browse beside everything
else. The game will open a loose file only when it is OGG Vorbis, so an MP3 or a WAV is re-encoded
on the way in. The tab lists every file it found and says plainly why anything was skipped, which is
usually an `.ogg` that turned out to hold Opus rather than Vorbis. A name too long for the game's 31
character slot field no longer costs you the track.

Each of your tracks gets a **Loop from** point there too, in seconds, the same thing every soundpack
track carries. Leave it at 0 and the whole song repeats, intro and all; set it past the intro and it
loops the way the game's own music does.

**New soundpack**, at the top of Browse, builds one of your own: tick the tracks you want in the
**Pack** column, choose which screen each one plays on, and save - it lands beside the packs that
ship with the mod and travels with Export.

### Performance

Its own window, opened from Config. It exists because the engine's frame pacing has a specific,
findable problem: the game runs its message pump on one thread and its frame on another, and every
frame the frame thread blocks on a message that only the pump can answer while the pump is asleep.
Windows also takes back the millisecond timer resolution while the game sits in the background,
which is what an alt-tab leaves behind.

Three options, each with its real trade-off written next to it, and two presets. The window reports
what is **actually** in force, read back from the device rather than from what the mod asked for.

The **Metrics** tab measures. Frame interval with its spread, a quarter-millisecond histogram around
the target, two-cluster detection for the judder a median cannot see, how long Present blocks, and a
paste-ready summary for bug reports.

An earlier build of this mod made the game feel worse, and did so by default: it added a second back
buffer for everyone and dragged high-refresh monitors down to 60 Hz in fullscreen, which cost a
frame of input latency and bought nothing. It no longer touches the display unless asked.

### POTATO MODE

The **POTATO MODE** tab of the Performance window, for a machine that cannot hold 60.

**The stage still draws at every level, and none of this touches the picture's size.**

| Level | Draws at | Also |
|---|---|---|
| Off | the game's own Display option | nothing |
| Balanced | 960x540 | back buffer multisampling off, the frame handshake waited on instead of the clock |
| Potato | **480p, 360p, 240p or 144p** | and Character Visual Improvements off |

Everything the mod draws - the overlay, the frame meter, the hitbox boxes and the origin cross -
keeps the size it has always had, instead of growing along with the stretch.

Pick Potato and a second row appears with the four sizes: `854x480`, `640x360`, `426x240`,
`256x144`. It is only offered while Potato is the level, because a size that does nothing because
another level is selected reads as broken.

**The size is a size, not a fraction of your window.** 640x360 stays 640x360 whether the window is
720p or 1440p, so you can make the window as large as you like and the game still only ever draws
that many pixels - Direct3D stretches the result to fill it. The engine draws exactly as it always
does, so nothing in it has to know and nothing can end up in the wrong place; the picture just gets
soft, the way a low resolution looks on a flat panel. 640x360 in a 720p window is a quarter of the
pixels to blend, scale and scan out; in a 1440p window it is a sixteenth.

**Off means off.** The mod stops touching the size entirely and the window goes back to whatever the
game's own Option -> Display says.

**Windowed and borderless only**: exclusive fullscreen has to name a display mode your adapter really
has, and choosing one for you is how a mod ends up owning your monitor. Set the size here and pick
fullscreen in the game's own menu if you want both - the game will use its own mode there.

The drawing size takes effect the next time the game builds its display: restart it, or touch any
video option in its own menu. Everything else on the tab is immediate.

*Character Visual Improvements* is the game's own option. It selects the filter techniques in the
character shader, which resolve nine palette lookups per character pixel instead of one, and what it
buys is a blur one source texel wide - about one screen pixel at 720p. Turning it off is the
cheapest real win on a weak card, and the mod holds it off, because the game's own options screen
writes the same setting back.

*Back buffer multisampling* costs nothing to lose: a Direct3D 9 texture cannot be multisampled at
all, so the game's Antialias never reaches a sprite edge - the only thing it can touch is the one
quad the finished frame is drawn with, whose only edges are the edges of the screen. Raising the
internal resolution above 100% is the only anti-aliasing this engine can be given, and the ini still
allows it.

**Nothing here reaches the simulation.** The match is the same match, nobody online can tell, and
none of it is a training tool. It is only how the frame is drawn.

The game builds its render targets once, so a resolution change takes effect after a restart, or
after you touch any video option in the game's own menu. Everything else is immediate. The tab
reports what is **actually** in force - the engine's own size, the drawing space, the largest render
target seen, and whether every patched site verified - so when a request has not taken, it says so
rather than leaving you to guess.

If anything looks wrong, set the level back to Off. The mod puts every byte it changed back.

### Improvements

The **Improvements** tab of the same window, and POTATO MODE the other way round: the frame is drawn
*larger* than your window and Direct3D fits it back down, so every edge is sampled several times over.

| Level | Draws at |
|---|---|
| Off | the game's own Display option |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

**Be clear about what it does and does not do.** The game rasterises the characters and the stage
into five render targets of a fixed 1280x720 before any of this, and that is untouched - a sprite
gains no detail from it. What it cleans up is everything drawn straight into the back buffer: the
HUD, the menus, the composite's edges, and the mod's own overlay. It is supersampling, not a higher
internal resolution.

It costs fill rate in proportion to the size - 4K is nine times 720p - so it is for a machine with
headroom, which is why it lives on its own tab. **The tab is only offered while POTATO MODE is Off**,
because the two settle the same drawing size from opposite ends.

**Sharpening** is on the same tab and is the more useful half. The game rasterises everything at
1280x720 and the composite blows that up to your window with a linear filter, so the softness you see
is in the upscale rather than in the art. This puts the edge contrast back - contrast adaptive, so it
follows edges instead of ringing them - and it is drawn over the finished frame from `Present`, which
means it patches nothing, reads none of the game's shaders and takes effect the moment you move the
slider. 40-60%% is the useful range. Measured on a real frame it raises mean edge gradient by about
47%%.

Windowed and borderless only, and like POTATO MODE it takes effect the next time the game builds its
display: restart it, or touch any video option in its own menu.

### Shaders

A tab of its own, next to Improvements. Everything on it runs over the picture and none of it touches
the simulation, the inputs or anything an opponent can see.

**Every stage is off by default and every one of them has an explicit Off.** While they all are, the
chain does not read the back buffer at all - the frame is the game's own.

- **Upscale filter** - Off, bicubic, Lanczos or FidelityFX EASU, substituted for the engine's own
  bilinear stretch to your window. EASU reads the gradient of the neighbourhood and stretches its
  kernel along the edge it finds, which turns a diagonal back into a line instead of a staircase.
  Needs a back buffer larger than 1280x720, so raise the present size with it.
- **Anti-aliasing** - FXAA in five steps. Multisampling is not offered because it cannot reach this
  game: a Direct3D 9 texture cannot be multisampled and the whole scene is drawn into textures.
- **Bloom** - the bright parts of the frame cut out, blurred at a quarter of the size and screened
  back on. The one setting here that reads as lighting rather than as filtering; the neon in these
  stages and the glow on EXS and super effects are what it is for.
- **Sharpening** - contrast adaptive, or FidelityFX RCAS to pair with the EASU upscale.
- **Colour and display** - brightness, contrast, gamma, saturation, vibrance, warmth, vignette,
  scanlines and dither.
- **Shader packs** - drop a shader in `UNI2-IM/Shaders` and pick it; it is compiled on selection and
  runs last in the chain. HLSL (`.hlsl`, `.ps`) is compiled as it stands, a ReShade `.fx` and
  libretro or Shadertoy GLSL (`.slang`, `.glsl`, `.frag`, `.fsh`) are translated on the way in.
  Compilation needs `d3dcompiler_47.dll`, which ships with Windows and with Proton; without it the
  rest of the tab is unaffected.

**Everything off** is a button on both this tab and Improvements. It puts the present size, all five
stages, the upscale filter, the back buffer's multisampling, Character Visual Improvements, the empty
stage and POTATO MODE back to the game's own in one click - after it the mod draws nothing into the
frame and reads nothing out of it.

#### Installing a shader

1. Drop the file in `<game folder>\UNI2-IM\Shaders` - the top of that folder, not a subfolder of it.
   The name does not matter, the extension does.
2. In game, open the overlay (F1) and go to **Graphics → Shaders**.
3. At **Shader pack**, press **Rescan** if the game was already running, then pick the file from the
   drop down.
4. It is compiled the moment you pick it, and the line underneath says what happened -
   `crt-lottes.slang compiled and running - GLSL fragment stage, one pass`. Pick **Off** to stop.

The pick is saved as `[Graphics] ShaderPack`, so it comes back next launch. Nothing is installed,
unpacked or registered: a shader is one file, and deleting it removes it.

| Extension | Read as |
|---|---|
| `.hlsl` `.ps` | HLSL, compiled exactly as written |
| `.fx` | ReShade |
| `.slang` | libretro / RetroArch |
| `.glsl` `.frag` `.fsh` | libretro, Shadertoy, or plain OpenGL GLSL |

Where to get them: **ReShade** - [reshade-shaders](https://github.com/crosire/reshade-shaders), the
`Shaders` folder, raw `.fx`; the single pass ones (colour grades, grain, vignettes, tone maps) land.
**libretro** - [slang-shaders](https://github.com/libretro/slang-shaders), raw `.slang`; `handheld/`
and `misc/` are full of single pass files, and the `.slangp` presets are chains with nothing here to
chain. **Shadertoy** - copy the editor's code into a `.glsl`; anything that only reads `iChannel0`
as the screen works.

#### Why they have to be translated

The game is Direct3D 9. A shader that runs here must be HLSL compiled to pixel shader 3.0, and there
is one slot for it, over the finished frame.

A ReShade `.fx` is HLSL but it is not a pixel shader - it is a small program describing uniforms,
annotations, textures, samplers and techniques made of passes, which ReShade's own runtime resolves
before anything reaches a compiler. A libretro `.slang` is Vulkan GLSL and a `.glsl` is OpenGL GLSL;
Direct3D cannot compile GLSL at all, and RetroArch puts them through a compiler chain of its own.
Neither can be handed to D3D9 as it stands, so the mod rewrites them: uniforms become their default
values, samplers become the frame, the resolution and time uniforms become the two constants above,
and the GLSL is rewritten as HLSL. The alternative is shipping a full effect runtime - a GLSL
compiler, multi-pass rendering and its own render targets - which is ReShade and RetroArch, not a
training mod.

**One pass over the finished frame is the whole budget.** A shader that wants a second pass, a
lookup texture, the depth buffer or the previous frame will translate and then be wrong; the big
multi pass CRT shaders - crt-royale, crt-guest-advanced, most of libretro's `crt/` - are exactly
that. The single pass ones are the ones that work.

The translation is written to `UNI2-IM\Shaders\Translated\<file>.hlsl`, and that file is what the
compiler was actually given. When something fails, the tab prints the compiler's own error and says
the line numbers belong to that copy: open it, look at the block of `#define`s at the top to see
what each of the original's uniforms was replaced with, fix it there, and save the result into
`Shaders` as an `.hlsl` of your own.

| Format | What it is given |
|---|---|
| `.fx` | `BUFFER_WIDTH`, `BUFFER_HEIGHT`, `BUFFER_RCP_WIDTH`, `BUFFER_RCP_HEIGHT`, `BUFFER_PIXEL_SIZE`, `BUFFER_SCREEN_SIZE`, `BUFFER_ASPECT_RATIO`, `ReShade::BackBuffer`, `ReShade::PixelSize`, `ReShade::ScreenSize`. Each `uniform` becomes its default value; one with a `source` of `timer` or `framecount` becomes `FrameTime`. Every sampler reads the frame, the depth buffer reads 1.0, and the first technique's `PixelShader` is the pass |
| `.slang` | `SourceSize`, `OriginalSize`, `OutputSize` and `FinalViewportSize` as `float4(w, h, 1/w, 1/h)`, plus `FrameCount`, `FrameDirection` and `MVP`. Each `#pragma parameter` becomes its default, the UBO and push constant blocks fold away, every sampler reads the frame, and the varying that carried the coordinate becomes the one this pass draws with. Only the fragment stage is kept |
| Shadertoy | `iResolution`, `iTime`, `iTimeDelta`, `iFrame`, `iMouse`, `iDate` and `iChannel0` to `iChannel3`, with `fragCoord` in pixels and y up |
| plain GLSL | A sampler uniform becomes the frame, an `in` or `varying` becomes the coordinate, an `out` or `gl_FragColor` becomes the result, and a uniform whose name looks like a resolution or a time is answered from `FrameSize` and `FrameTime`. Everything else becomes 0. The legacy `#ifdef FRAGMENT` form is understood |

The rewrite is lexical, not a compiler: GLSL types and intrinsics are mapped to their HLSL names,
single argument vector constructors become casts, and identifiers HLSL reserves (`line`, `sample`,
`matrix`) are renamed.

#### Writing a shader pack

Eighteen examples are written into `UNI2-IM/Shaders` the first time a new version starts, next
to a `README.txt` that repeats all of this. Twelve are HLSL: `01_passthrough` (the skeleton to
copy), then `02_grayscale`, `03_sepia`, `04_invert`, `05_posterize`, `06_pixelate`,
`07_chromatic`, `08_film_grain`, `09_vhs`, `10_lcd_grid`, `11_outline` and `12_crt` - curvature,
scanlines, a phosphor mask, bleed and vignette. Each keeps its settings as `#define` lines at the
top; edit them and reselect the pack to compile it again.

The other six are one per format the mod takes, so you can read each one next to what the
`Translated` folder makes of it: `13_reshade_tonemap.fx` (annotated uniforms, a sampler, a
technique), `14_slang_scanlines.slang` (`#pragma parameter`s, a UBO, two stages),
`15_shadertoy_ripple.glsl` (one `mainImage`), `16_bleach_bypass.ps` (HLSL under the other
extension), `17_dot_matrix.frag` (modern GLSL - `in`, `out`, `texture()`) and
`18_bloom_glow.fsh` (old GLSL - `varying`, `gl_FragColor`, `texture2D`).

Files you add or edit are never overwritten, and a file you delete stays deleted until the mod
updates. The sources are in `resource/shaders/examples`.

A pack is one file: entry point `main`, target `ps_3_0`, one pass, pixel shader only. The mod
binds three things and nothing else:

```hlsl
sampler2D Frame  : register(s0);   // the frame so far
float4 FrameSize : register(c0);   // xy = 1/width, 1/height   zw = width, height
float4 FrameTime : register(c1);   // x  = seconds since load  y = frames since load
```

There is no vertex shader of yours, no second pass, no copy of the previous frame and no depth.

`Frame` is sampled with **point** filtering, so reading straight through at `uv` is exact - which
is what a pixel art game wants - but a pack that bends the coordinates has to filter for itself.
`12_crt.hlsl` carries the four tap bilinear that does it.

### Memory debug

Off by default; set `[Debug] MemoryDebug = 1`, then **Ctrl+F1**. Raw readouts and the search tools
used to build the rest of the mod: entity list, hitbox dump, move state, a global scanner, a diff
search, a pointer follower and a struct viewer.

## Coming later

- **Palettes in the lobby**, not only in the match.

## The ini file

`UNI2_IM.ini` sits in the `UNI2-IM` folder next to the DLL, and the mod completes it on every run:
a missing key or section is appended with its default, an edited one is left exactly as it is, and
the whole file can be deleted to start over. So the file always lists every setting this build
understands, and keys added by a new version turn up in it on the next launch.

### `[Mod]`

| Key | Default | What it does |
|---|---|---|
| `DinputDllWrapper` | empty | Full path to another `dinput8.dll` to chain-load. Empty uses the system one. |
| `CheckForUpdates` | `1` | Asks GitHub once, on a thread of its own, whether a newer release exists. Nothing is downloaded until you press **Update now**. |
| `SettingsRevision` | `2` | Which release's defaults this file was last brought up to. A lower number lets the mod correct a setting whose old default turned out to be unsafe. Never edit it by hand. |

### `[Keybinds]`

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

### `[PadKeybinds]`

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

### `[Input]`

| Key | Default | What it does |
|---|---|---|
| `KeyboardSeat` | `0` | Which player number the keyboard is: 0 leaves the game alone, 1 puts your own keys on 1P, 2 on 2P. |
| `KeyboardSeatRouteSides` | `1` | Whether the seat also writes both sides' controller slots every frame of a local match. |

### `[Training]`

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

### `[FrameMeter]`

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

### `[Palette]`

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

### `[Netplay]`

| Key | Default | What it does |
|---|---|---|
| `SafeOnline` | `1` | While a netplay session is up the mod writes nothing anybody else receives and calls nothing the netcode owns. Everything below still runs in a room. Off, each switch decides for itself again - which is what the mid-match disconnects were traced to. |
| `RoomRosterFix` | `1` | The game removes a room member only on an exact `Left`, so `Disconnected`, `Kicked` and `Banned` leave a ghost behind. On, those are routed to the handler the game uses for `Left`. `SafeOnline` holds it back once a session is up, because a blip Steam reports as `Disconnected` would otherwise take the opponent out of the room mid-match. |
| `RepublishPingLocation` | `1` | Republishes your Steam ping location into the room every 30 s. The game publishes it once, on join, which is why rejoining "resets" the ping. Held back during a session. |
| `Diagnostics` | `0` | Asks GGPO for ping and frame advantage by calling a method on the game's own backend from the render thread - the netcode thread's object. Off by default for that reason, and throttled to once every twenty frames when on. The rollback and frame counters work either way; those are plain reads. |
| `SharePalettes` | `1` | Sends your palette to the other player over the mod's own Steam channel. It shares a connection with the rollback traffic, so it is sent once when the opponent is not known to be running the mod and three times when they are. |
| `UnloadPatchOnline` | `1` | A patch is battle data the other side does not have. On, going online unloads it and says so, which turns a desync into a restart. Off, the mod only warns. |

### `[Video]`

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

### `[Music]`

| Key | Default | What it does |
|---|---|---|
| `KeepMenuMusic` | `1` | Keep the menu music playing across Options, Customize and Gallery. The game's own menu BGM chooser rebuilds the track from the start unless it is still running with the same id when you come back, and those screens pause it on the way in, so it always restarts. On, the mod holds the paused track for the chooser and resumes it. 0 is the game's own behaviour. Also a checkbox in the Music section. |

### `[Graphics]`

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

### `[Overlay]`

| Key | Default | What it does |
|---|---|---|
| `UiScale` | `1.0` | Overlay scale. 1.0 is native. |
| `FontPath` | empty | A `.ttf` for the overlay. Empty picks the first scalable face the system has - Segoe UI on Windows, usually DejaVu Sans under Proton. |
| `FontSize` | `16.0` | Its size in pixels before scaling. |
| `DpiAware` | `0` | Tell Windows the game handles its own scaling. Off, a display scale above 100% makes Windows render the window small and stretch it, which is a second blur over everything. Has to be set before the game makes its window, so it needs a restart, and the Config tab reports whether it took. |
| `Notifications` | `1` | The line that slides across the top when the mod loads. 0 silences it. |
| `BlockGameMouse` | `0` | Stop the game seeing the mouse at all, so clicking the overlay cannot disturb it. |
| `DrawWhileGamePaused` | `0` | Keeps the hitbox viewer and the frame meter up while the game's own pause menu is open. Off, both hide with the battle tick. |

### `[Debug]`

| Key | Default | What it does |
|---|---|---|
| `MemoryDebug` | `0` | Loads the Memory debug window, opened with Ctrl+F1. |
| `Profiler` | `0` | Frame interval and per-section timing, shown in the Performance window's Metrics tab. |
| `MeterTrace` | `0` | The frame meter's diagnostic capture and its CSV. |

`Logging = 1` is what turns logging on, and nothing is written without it. It is the first thing to
ask for when someone reports that the mod does nothing: the log records the startup trail, every
hook the mod installed and where, and anything that faulted.

### `[Compat]`

| Key | Default | What it does |
|---|---|---|
| `WineSafeMode` | `-1` | `-1` automatic - on under Wine/Proton, off on Windows. `1` forces it on, `0` forces it off. On, the mod leaves the host's presentation and scheduling alone: no fullscreen refresh rewriting, no power throttling opt-out, no `Sleep` substitution. Set `0` on Linux to find out whether one of those three is what is misbehaving. |

## Building

Needs **Visual Studio 2026** - the project targets platform toolset **v145**, which the 2022
install does not have and fails on with `MSB8020`. Everything else is vendored under `depends/`
(Dear ImGui, MinHook). The DirectX SDK is not needed; `d3d9.h` ships with the Windows SDK.

```
MSBuild UNI2_IM.slnx /p:Configuration=Release /p:Platform=Win32
```

`UNI2_IM.sln` is kept beside it for older tooling; both build the same project.

Output is `bin\Release\dinput8.dll`. **Win32 only** - the game is 32-bit. Add
`/p:EnableLogging=true` for a runtime log next to the DLL. The game locks the DLL while it runs, so
close it before building over an installed copy.

## Layout

```
src/Core/       DLL entry, dinput8 forwarding, settings, logging, crash dumps
src/Hooks/      Signature scanning, MinHook wrappers, RTTI vtable lookup, input hooks
src/D3D9/       Direct3DCreate9 and device vtable hooks, present tuning
src/Overlay/    ImGui setup, window registry, individual windows
src/Game/       Game memory layer, structures and resolved pointers
src/Training/   Frame stepper, frame meter, player control, input lag meter
src/Palette/    Palette identity, painting and sharing
```

## Credits

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) - architecture reference
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) - HA6 / CG / PAL format ground truth
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) - modding documentation
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

### Special thanks

- Pescador Cearense
- Eon
- Listentothebirds - Rafael
- Willyofruit
- Sky Leite
- Excel
- ZateFGC
- Yorezordd (Velho fudido)
- Thiago
- Tanasinn [AZ]
- Licensed Grappler
- Anklegator

---

# Português

Mod de treino e conveniência para **UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`).

Ele carrega como um proxy de `dinput8.dll` e desenha uma interface Dear ImGui dentro do renderizador
Direct3D 9 do jogo.

## Sobre o online

**Nada neste mod foi feito para dar vantagem a ninguém no online, e nada nele dá.**

Toda ferramenta de treino capaz de alterar o que a simulação faz — avançar quadro a quadro,
congelar, dirigir um personagem na mão, os scripts de dummy — é travada nos modos offline. A trava é
o próprio tráfego ponto a ponto do jogo: enquanto ele tiver enviado um pacote a um oponente nos
últimos três segundos, essas ferramentas se recusam a rodar. O jogo usa netcode de rollback GGPO, e
qualquer coisa que toque o estado da simulação durante uma partida a dessincroniza.

O que roda online é cosmético e apenas de leitura: as paletas personalizadas, que viajam ao lado do
jogo pela Steam em vez de passar pelo netcode e não têm como afetar a partida, e as opções de
desempenho, que só mudam como o quadro chega ao seu monitor.

Se você encontrar aqui algo que dê vantagem numa partida de verdade, isso é um bug. Reporte.

## Instalando

Copie `dinput8.dll` para a pasta do `uni2.exe`:

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

**F1** no jogo abre a interface. Para desinstalar, apague `dinput8.dll`.

O `UNI2_IM.ini` é criado com os padrões na primeira vez que o mod roda, na pasta `UNI2-IM` ao lado
da DLL. A partir daí ele se conserta sozinho: a cada execução, toda chave ou seção que estiver
faltando é acrescentada com o valor padrão, e nada que você editou é tocado. Uma versão nova que
adicione configurações passa a acrescentá-las no seu arquivo, e um arquivo que você reduziu a duas
linhas na mão é preenchido de volta. Apague-o para voltar ao padrão. Cada chave está documentada em
[The ini file](#the-ini-file).

O medidor de quadros desenha com a arte de painel e a fonte do próprio jogo. O mod extrai esses
arquivos do arquivo `d` do jogo para `UNI2-IM\Assets` na primeira execução, então não há nada para
extrair na mão e nenhum dado do jogo no download. Apague a pasta e ela é refeita; se o arquivo não
puder ser lido, o medidor continua funcionando, desenhado em cores chapadas.

Para encadear outro wrapper de `dinput8.dll`, ponha o caminho completo dele em
`[Mod] DinputDllWrapper`.

### Linux e Steam Deck (Proton)

Copie o `dinput8.dll` para o lado do `uni2.exe` exatamente como no Windows e faça o único passo que
o Windows não precisa - avisar o Wine para carregá-lo:

1. Na sua biblioteca Steam, clique com o botão direito em **UNDER NIGHT IN-BIRTH II Sys:Celes** e
   abra **Propriedades**.
2. Em **Geral**, no campo **Opções de inicialização**, ponha esta linha exatamente assim:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

3. Abra o jogo e aperte **F1**.

É só isso. Nada é renomeado e nada mais é copiado.

**Por que é necessário.** O Wine escolhe qual `dinput8.dll` carregar por uma configuração do prefixo,
não pela pasta em que o arquivo está, então sem essa linha a DLL fica ao lado do `uni2.exe` e
simplesmente nunca é carregada - é isso que o relato "o mod não faz nada no Linux" quer dizer. `n,b`
quer dizer *primeiro o nativo, depois o embutido*: a cópia do mod carrega, e o `dinput8` do próprio
Wine continua respondendo tudo o que o mod repassa para ele, que é o motivo de o controle do jogo
continuar funcionando.

O Proton 9 e mais novos já fazem isso sozinhos com o `dinput8.dll` de um mod, então neles a linha não
muda nada e pode ficar. O Proton antigo não faz, e sem ela não carrega o mod.

No Linux o mod liga sozinho o **modo de compatibilidade**: sem reescrever a taxa de atualização em
tela cheia, sem opt-out de power throttling e sem substituir o `Sleep`. Esses três são ajustes para
o agendador e o compositor do Windows; no Linux o DXVK e o kernel já fazem esse trabalho com
informação melhor. Ponha `[Compat] WineSafeMode = 0` para tê-los de volta - e para descobrir se um
deles é o que está atrapalhando, caso algo esteja.

Se não acontecer absolutamente nada, procure uma pasta `UNI2-IM` ao lado do `uni2.exe`. Se ela não
existir, a DLL nunca foi carregada, então o problema é o passo acima e não o mod. Para tirar um log
de uma máquina onde ele carrega, crie o `UNI2-IM/UNI2_IM.ini` na mão com só estas duas linhas e abra
o jogo uma vez - o mod preenche o resto do arquivo sozinho:

```ini
[Debug]
Logging = 1
```

### RivaTuner Statistics Server, MSI Afterburner e outros overlays

Dois overlays no mesmo jogo são dois motores de hook nas mesmas funções do Direct3D, e o final
costuma ser um escrevendo o salto dele por cima do do outro. O RTSS verifica se o salto dele ainda
está lá e o repõe quando não está, o que leva junto o hook do outro motor - o overlay do mod desenha
um quadro e nunca mais, ou o jogo fecha na inicialização.

O mod não escreve mais por cima de ninguém. Cada hook segue a cadeia de saltos que já está na frente
da função e se instala no fim dela, que é exatamente o que o autor do RTSS pede que terceiros façam.
Os dois overlays acabam numa cadeia só que funciona, e a ordem de carregamento deixa de importar. É
a mesma mudança que fez o overlay da Steam, que hooka as mesmas funções, conviver bem também.

Se mesmo assim algo der errado, ponha `[Debug] Logging = 1` no `UNI2_IM.ini` e rode o jogo uma vez: o
log em `UNI2-IM/Logs` diz qual hook foi e o que aconteceu com ele, e a janela Debug mostra o mesmo
estado ao vivo. Duas opções do RTSS resolvem o resto:

- **Settings / General / Injection properties, "Use Microsoft Detours API hooking".** Isso muda o
  RTSS para um modelo de hook feito para conviver com outros motores.
- **No perfil do jogo no RTSS, Application detection level, None.** Aí o RTSS deixa o jogo em paz de
  vez - e o overlay dele junto.

## Funções

### Interface

**F1**. Tudo fica numa janela só, com três seções — Training, Custom e Config — mais janelas
separadas para o visualizador de hitbox, a legenda do medidor de quadros, o Player Control e o
Performance.

Enquanto você digita em qualquer campo de texto, o teclado pertence à interface e o jogo não recebe
nada; o mesmo vale enquanto uma tecla está sendo capturada. Uma tecla ainda pressionada quando o
campo é liberado fica mascarada até você soltá-la, para que nada saia como golpe.

### Visualizador de hitbox

**F2**. Desenha todas as caixas que a engine tem, para personagens **e** projéteis, nas cores do
próprio jogo. A decoração é filtrada pela flag `_Exist_NoHantei` da própria engine, em vez de
adivinhada, então o que você vê é o que o jogo lê.

Não existe caixa para agarrão, para o D Shield nem para proximity guard — o jogo não tem. A região
de captura de um agarrão é uma caixa de ataque comum, num quadro que carrega atributo de agarrão.

### Medidor de quadros

**F3**. Medidor com uma faixa por estado, a contagem de quadros impressa dentro de cada faixa
concluída, a troca totalizada numa linha própria e uma linha de status nomeando toda invencibilidade
em vigor naquele quadro. A vantagem de quadros é medida contra o campo de vantagem do próprio jogo,
e não inferida da animação.

Ele começa centralizado perto da base da tela e pode ser arrastado com o mouse. É desenhado direto no
back buffer e não tem nenhuma detecção de clique da interface, então um clique que caia sobre ele o
move, independentemente do que você estivesse fazendo.

### Pausa e avanço de quadro

**F5** pausa, **F6** avança um quadro. Segurar F6 repete.

Há dois modos de congelamento. O *Tick stop* suprime o quadro inteiro do jogo e é o padrão. O
*Hitstun Stop* reaproveita o hitstop da própria engine, o que mantém os menus vivos mas desenha os
efeitos errados — isso é inerente à técnica, não um defeito.

O auto pause pode parar o jogo sozinho num ataque, num golpe com armadura ou num acerto específico
do combo, e voltar depois de uma contagem regressiva.

### Player Control

Uma janela própria. Mostra o que os dois lados estão inserindo, ao vivo, como um direcional numpad e
quatro luzes de botão. Você pode apontar seu próprio controle ou teclado para qualquer um dos
personagens, segurar uma direção ou um botão no dummy, dar um toque de alguns quadros ou rodar um
script escrito para cada lado.

Ele também mede o input lag: o tempo de relógio entre um toque físico e a mudança do campo de input
daquele personagem. O teclado e cada controle são amostrados cerca de mil vezes por segundo numa
thread própria, em vez de lidos da varredura de uma vez por quadro do jogo, que quantizaria toda
resposta em 16,7 ms e não mediria nada. As duas APIs de controle que o jogo usa estão cobertas.

### Lado do teclado

*Config → Keyboard.* Escolha se o teclado joga como **1P** ou **2P**: ele passa a ser um jogador
próprio, com as teclas que você já configurou. Feito para jogo local em torneio, onde os dois
jogadores dividem a mesma máquina e um deles está no teclado.

O jogo dá ao teclado e ao primeiro controle o mesmo número de jogador, então no versus local os
dois movem o mesmo personagem. Escolher um lado aqui move o **controle** para o outro; o teclado
fica exatamente como está, com as teclas que você já configurou.

Ele nunca escreve nas suas configurações de tecla, e nunca move o teclado entre os dois jogadores
de teclado do jogo — as duas coisas foram tentadas e as duas quebraram algo. Se você tem um segundo
jogador de teclado configurado nas opções do jogo, aquelas teclas respondem no lado do controle;
coloque *Keyboard Player Number* em 1 lá para desligá-las.

*Hold the side during a match* continua escrevendo as portas dos dois lados durante a partida local,
para que o lado escolhido seja o lado que você recebe. Desligue para deixar o jogo decidir.

Nada disso roda online.

### Paletas

Qualquer cor em qualquer um dos personagens, aplicada ao vivo. Uma cor pertence ao personagem, e não
a um lado da tela, então ela o acompanha mesmo numa troca de lado; e o que cada personagem veste é
lembrado e recolocado automaticamente na próxima vez.

As paletas são salvas como arquivos `.pal` comuns — o formato do próprio jogo, que o Hantei-kun
também escreve — em `UNI2-IM\Palettes\<personagem>`, com nome, autor e descrição. As cores de efeito
fazem parte da paleta e são salvas e carregadas junto. A lista de paletas do personagem tem uma
entrada **Default**, que devolve as cores do próprio jogo.

No online, sua paleta é enviada ao outro jogador ao lado do jogo, pela Steam. **See the other
player's colours** decide o lado dele numa única chave: ligada, a paleta dele é lida quando chega e
vestida, então você vê o que ele escolheu; desligada, os pacotes dele são descartados e o lado dele
fica do jeito que o jogo dá. A sua é enviada de qualquer forma, e quem não tem o mod não vê nada.

**Palette Nativa** é outra coisa: o customizador de cores do próprio jogo, controlado pela interface.
Ele monta uma cor a partir das paletas de fábrica do personagem, uma por parte, então não pode ser
qualquer cor — mas o jogo a salva e **todo** oponente a vê, com mod ou sem.

### Player Card

Edita o cartão que o jogo publica para o seu oponente: as quatro camadas da placa e o título. O
título é texto livre — as três palavras da loja são apenas o estado do seletor — então qualquer
frase escrita ali é salva e chega ao oponente exatamente como foi escrita.

### Seletor de BGM

Em uma janela própria, aberta pelo item **Music** no menu do mod.

Toda tela com música passa por um único ponto de entrada no jogo, então qualquer uma pode receber
outra faixa: o tema de batalha de um personagem, a seleção de personagem, a tela de VS, um menu. O
jogo traz três temas de confronto; isto é a mesma ideia sem o limite.

**Soundpacks** instala a trilha inteira de um jogo de uma vez e aponta todas as telas para ela.
**Get OST from French-Bread games** lê a trilha de uma cópia que você já tem — aponte para a pasta
com `UNIclr.exe` ou `UNIst.exe`, `MBTL.exe`, ou `MBAA.exe` — e instala aqui com títulos das músicas
e pontos de loop. Nada é baixado, e nenhum áudio acompanha o mod. Rodar duas vezes no mesmo jogo
substitui o que foi adicionado em vez de duplicar. Export e Import levam e trazem seus packs em um
único zip, para um amigo ter o mesmo conjunto sem repetir nada disso.

**Browse** lista todas as faixas que o jogo pode tocar, com busca e filtro por origem, e Play para
iniciar uma e segurá-la — o jogo recupera a música dele quando você aperta Stop. O **Randomizer**
entrega uma faixa aleatória dessa lista toda vez que o jogo pede música, e cada faixa tem um
interruptor, então o que você desliga nunca é sorteado.

**Rules** é a versão manual: toque esta faixa neste confronto, para este personagem, ou no lugar
desta tela. As regras também podem ser exportadas e importadas, e importar soma à sua lista em vez
de substituí-la.

Sua própria música também funciona: coloque arquivos `.ogg` em uma pasta dentro de `UNI2-IM/Music`
e eles aparecem na lista junto com o resto.

### Performance

Janela própria, aberta pelo Config. Ela existe porque o ritmo de quadros da engine tem um problema
específico e localizável: o jogo roda a fila de mensagens numa thread e o quadro em outra, e a cada
quadro a thread do quadro bloqueia numa mensagem que só a fila pode responder — enquanto a fila está
dormindo. O Windows também retoma a resolução de timer de 1 ms enquanto o jogo fica em segundo
plano, que é o que um alt+tab deixa para trás.

São três opções, cada uma com a sua contrapartida real escrita ao lado, e dois presets. A janela
relata o que está **de fato** em vigor, lido de volta do dispositivo, e não o que o mod pediu.

A aba **Metrics** mede: intervalo entre quadros com a sua dispersão, um histograma de um quarto de
milissegundo em torno do alvo, detecção de dois agrupamentos para o tremor que uma mediana não vê,
quanto tempo o Present bloqueia, e um resumo pronto para colar num relatório de bug.

Uma versão anterior deste mod deixou o jogo pior, e por padrão: ela acrescentava um segundo back
buffer para todo mundo e puxava monitores de alta taxa de atualização para 60 Hz em tela cheia, o
que custava um quadro de latência e não trazia nada em troca. Ele não mexe mais no display sem ser
solicitado.

### POTATO MODE

A aba **POTATO MODE** da janela Performance, para uma máquina que não consegue segurar 60.

O jogo rasteriza todo quadro em cinco render targets de 1280x720 fixos e só depois escala o
resultado para o seu display, então quase todo pixel que ele paga é um desses. O POTATO MODE abaixa
esse tamanho. Metade é um quarto dos pixels nas cinco passagens; um quarto é um dezesseis avos.
**O cenário continua sendo desenhado em todos os níveis** — a imagem fica suave, não fica vazia.

| Nível | Desenha em | E também |
|---|---|---|
| Off | o que a opção Display do jogo pedir | nada |
| Balanced | 960x540 | multisampling do back buffer desligado, espera pelo handshake de quadro |
| Potato | 480p, 360p, 240p ou 144p | e Character Visual Improvements desligado |

**É um tamanho, não uma fração da janela.** 640x360 continua 640x360 com a janela em 720p ou em
1440p, então você pode deixar a janela do tamanho que quiser que o jogo continua desenhando só essa
quantidade de pixels — o Direct3D estica o resultado até preencher. A engine desenha exatamente como
sempre desenhou, então nada nela precisa saber e nada pode acabar no lugar errado; a imagem só fica
suave.

**Off é off**: o mod para de mexer no tamanho e a janela volta ao que a opção Display do próprio jogo
disser.

Só em janela e janela sem borda. O tamanho vale a partir da próxima vez que o jogo montar o display —
reinicie, ou mexa em qualquer opção de vídeo no menu dele.

*Character Visual Improvements* é uma opção do próprio jogo. Ela seleciona as técnicas de filtro do
shader de personagem, que resolvem nove consultas à paleta por pixel em vez de uma, e o que isso
compra é um borrão de um texel de origem — cerca de um pixel de tela em 720p. Desligá-la é o ganho
real mais barato numa placa fraca, e o mod a mantém desligada, porque a tela de opções do jogo
reescreve a mesma configuração.

*Multisampling do back buffer* não custa nada perder: uma textura Direct3D 9 não pode ser
multisampled, então o Antialias do jogo nunca alcança a borda de um sprite. Subir a resolução
interna acima de 100% é o único anti-aliasing que esta engine aceita, e o ini continua permitindo.

**Nada aqui alcança a simulação.** A partida é a mesma partida e ninguém no online percebe.

O jogo constrói os render targets uma vez, então mudar a resolução só vale depois de reiniciar, ou
depois de mexer em qualquer opção de vídeo no menu do próprio jogo. O resto é imediato. A aba relata
o que está **de fato** em vigor e avisa quando um pedido não pegou. Se algo parecer errado, volte o
nível para Off: o mod devolve cada byte que alterou.

### Improvements

A aba **Improvements** da mesma janela, e é o POTATO MODE ao contrário: o quadro é desenhado *maior*
que a sua janela e o Direct3D encaixa de volta, então cada borda é amostrada várias vezes.

| Nível | Desenha em |
|---|---|
| Off | o que a opção Display do jogo pedir |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

**Seja claro sobre o que isso faz e o que não faz.** O jogo rasteriza personagens e cenário em cinco
render targets de 1280x720 fixos antes de tudo isso, e isso não é tocado — um sprite não ganha
detalhe nenhum. O que melhora é tudo que é desenhado direto no back buffer: o HUD, os menus, as
bordas da composição e o próprio overlay do mod. É supersampling, não resolução interna.

Custa fill rate proporcional ao tamanho — 4K é nove vezes 720p. **A aba só aparece com o POTATO MODE
em Off**, porque os dois decidem o mesmo tamanho por lados opostos. Só em janela e janela sem borda.

### Memory debug

Desligado por padrão; coloque `[Debug] MemoryDebug = 1` e use **Ctrl+F1**. Leituras cruas e as
ferramentas de busca usadas para construir o resto do mod.

## O que vem depois

- **Paletas no lobby**, e não apenas na partida.

## O arquivo ini

As chaves são as mesmas listadas na seção em inglês acima — [The ini file](#the-ini-file) — com os
mesmos padrões. Chaves ausentes assumem o padrão, então você pode apagar o que não está mudando, e o
arquivo inteiro pode ser apagado para recomeçar.

## Créditos

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) — referência de arquitetura
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) — referência dos formatos HA6 / CG / PAL
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) — documentação de modding
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

### Agradecimentos especiais

- Pescador Cearense
- Eon
- Listentothebirds - Rafael
- Willyofruit
- Sky Leite
- Excel
- ZateFGC
- Yorezordd (Velho fudido)
- Thiago
- Tanasinn [AZ]
- Licensed Grappler
- Anklegator

---

# 日本語

> **注意: この節は機械翻訳です。** 正確な内容は英語版を参照してください。
> (Machine translation. Refer to the English section for the authoritative text.)

**UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`) 用のトレーニング・利便性向上 MOD です。

`dinput8.dll` のプロキシとして読み込まれ、ゲームの Direct3D 9 レンダラー内に Dear ImGui のオーバー
レイを描画します。

## オンラインについて

**この MOD には、オンラインで有利になるように作られた機能はひとつもありません。**

シミュレーションの動作を変えうるトレーニング機能 — フレーム送り、停止、キャラクターの手動操作、
ダミースクリプト — はすべてオフライン専用に固定されています。判定にはゲーム自身の P2P 通信を使い、
直近 3 秒以内に対戦相手へパケットを送っていれば、これらの機能は動作を拒否します。本作は GGPO の
ロールバックネットコードを採用しており、対戦中にシミュレーション状態へ触れるものは同期ずれを起こし
ます。

オンラインで動作するのは外見に関わる読み取り専用の機能だけです。カスタムパレットはネットコードでは
なく Steam 経由でゲームと並行して送られるため対戦に影響を与えられず、パフォーマンス設定は映像が
モニターへ届くまでの経路を変えるだけです。

実際の対戦で有利になるものを見つけた場合、それは不具合です。報告してください。

## インストール

`dinput8.dll` を `uni2.exe` と同じフォルダーにコピーします。

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

ゲーム中に **F1** でオーバーレイを開きます。アンインストールは `dinput8.dll` を削除するだけです。

`UNI2_IM.ini` は初回起動時に既定値で DLL の隣の `UNI2-IM` フォルダーに作成されます。以降は自動的に補完
されます。起動のたびに、ファイルに無いキーやセクションが既定値で追記され、編集済みの値には一切触れま
せん。設定が増えた新しいバージョンでも、既存のファイルにその項目が追加されます。削除すれば既定値に戻り
ます。各キーの
説明は [The ini file](#the-ini-file) にあります。

### Linux / Steam Deck (Proton)

Windows と同じように `uni2.exe` の隣に `dinput8.dll` を置き、そのうえで Windows では要らない手順を一つ
だけ行います。Wine に読み込ませる指定です。

1. Steam のライブラリで **UNDER NIGHT IN-BIRTH II Sys:Celes** を右クリックし、**プロパティ**を開きます。
2. **一般**の**起動オプション**に、次の一行をそのまま入力します。

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

3. ゲームを起動して **F1** を押します。

これだけです。ファイル名の変更も、他のファイルのコピーも必要ありません。

**なぜ必要か**: Wine はどの `dinput8.dll` を読み込むかを、ファイルの置き場所ではなくプレフィックスごと
の設定で決めます。そのためこの一行がないと、DLL を `uni2.exe` の隣に置いても読み込まれません。「Linux
では何も起きない」という報告はこれです。`n,b` は*まずネイティブ、次に内蔵*という意味で、MOD 側が読み込
まれたうえで、MOD が受け流した呼び出しには Wine 自身の `dinput8` が答えます。ゲームのコントローラー入力
がそのまま動くのはこのためです。

Proton 9 以降は MOD 自身の `dinput8.dll` について同じことを既に行っているので、そこではこの一行を入れて
も何も変わらず、そのままにしておいて構いません。古い Proton は行わないため、これがないと MOD は読み込ま
れません。

Linux では MOD が自動的に**互換セーフモード**になります。フルスクリーンのリフレッシュレート書き換え、
電力スロットリングの opt-out、`Sleep` の置き換えを行いません。いずれも Windows のスケジューラーとデス
クトップコンポジター向けの調整で、Linux では DXVK とカーネルがより正確な情報で同じ仕事をしています。
`[Compat] WineSafeMode = 0` で元に戻せます。不具合の切り分けにも使えます。

何も起きない場合は、`uni2.exe` の隣に `UNI2-IM` フォルダーができているか確認してください。無ければ DLL
自体が読み込まれていないので、原因は MOD ではなく上の手順です。読み込まれている環境でログを取るには、
`UNI2-IM/UNI2_IM.ini` を次の 2 行だけで手動作成して一度起動してください。残りは MOD が自動で補完しま
す。

```ini
[Debug]
Logging = 1
```

### RivaTuner Statistics Server / MSI Afterburner などのオーバーレイ

同じゲームに二つのオーバーレイがあるということは、同じ Direct3D 関数に二つのフックエンジンが乗るという
ことです。片方がもう片方のジャンプを上書きしてしまうのが典型的な結末です。RTSS は自分のジャンプが残っ
ているかを定期的に確認し、無ければ書き戻すので、その際に相手のフックごと外れます。MOD のオーバーレイが
1 フレームだけ描かれて消える、あるいは起動時に落ちるのはこれが原因です。

この MOD はもう誰の上にも書き込みません。各フックは関数の先頭にすでにあるジャンプの連鎖をたどり、その
末尾に自分を入れます。これは RTSS の作者がサードパーティに求めている方式そのもので、両方のオーバーレイ
が一本の正しい連鎖に収まり、読み込み順は問題でなくなります。同じ関数をフックする Steam オーバーレイと
共存できるようになったのも同じ変更によるものです。

それでも問題が出る場合は `UNI2_IM.ini` の `[Debug] Logging = 1` を設定して一度起動してください。
`UNI2-IM/Logs` のログにどのフックがどうなったかが記録され、Debug ウィンドウにも同じ状態が表示され
ます。残りは RTSS 側の二つの設定で解決します。

- **Settings / General / Injection properties の "Use Microsoft Detours API hooking"**。他のエンジン
  との共存を前提としたフック方式に切り替わります。
- **このゲームの RTSS プロファイルで Application detection level を None に**。RTSS はゲームに一切触
  れなくなります（RTSS 自身のオーバーレイも出なくなります）。

## 機能

### オーバーレイ

**F1**。Training・Custom・Config の 3 セクションが 1 つのウィンドウにまとまり、ヒットボックス
表示、フレームメーターの凡例、Player Control、Performance は別ウィンドウになります。

テキスト欄に入力している間、キーボードはオーバーレイが占有し、ゲームには何も渡されません。キー
設定の取得中も同様です。欄を離れた時点で押されたままのキーは、離すまでマスクされます。

### ヒットボックス表示

**F2**。キャラクターと飛び道具の両方について、エンジンが持つすべてのボックスをゲーム自身の配色で
描画します。装飾はエンジン自身の `_Exist_NoHantei` フラグで除外しており、推測ではありません。

投げ、D シールド、近距離ガードにボックスは存在しません。投げの捕捉範囲は、投げ属性を持つフレームの
通常の攻撃ボックスです。

### フレームメーター

**F3**。状態ごとの帯、確定した帯の中に表示されるフレーム数、独立した行に出るやり取りの合計、その
フレームで有効な無敵をすべて記した状態行を表示します。フレーム有利はアニメーションからの推測では
なく、ゲーム自身の有利フレーム値と照合しています。

初期位置は画面下部の中央付近で、マウスでドラッグできます。バックバッファーへ直接描画されるため、
上をクリックすると何をしていても移動します。

### 一時停止とフレーム送り

**F5** で停止、**F6** で 1 フレーム進みます。F6 を押し続けると連続します。

停止方式は 2 つあります。既定の *Tick stop* はゲームのフレーム処理全体を止めます。*Hitstun Stop*
はエンジン自身のヒットストップを流用するためメニューは動き続けますが、エフェクトの描画が崩れます。
これは手法上の性質であり、不具合ではありません。

自動停止は、攻撃・アーマー付き技・コンボの指定ヒット数で自動的にゲームを止め、カウント後に再開でき
ます。

### Player Control

専用ウィンドウです。両サイドの入力をテンキー表記のレバーと 4 つのボタンランプでリアルタイムに表示
します。自分のパッドやキーボードをどちらのキャラクターへも割り当てられ、ダミーに方向やボタンを
押させ続けたり、数フレームだけ入力させたり、サイドごとに記述したスクリプトを実行したりできます。

入力遅延の計測も行います。物理的な入力から、そのキャラクター自身の入力フィールドが変化するまでの
実時間です。キーボードと各パッドは専用スレッドで毎秒約 1000 回サンプリングしています。ゲームの
1 フレームに 1 回のポーリングを読むと、すべての結果が 16.7 ms 単位に丸められて計測にならないため
です。ゲームが使う 2 種類のパッド API の両方に対応しています。

### キーボードのサイド

*Config → Keyboard*。キーボードを **1P** と **2P** のどちらで遊ぶか選べます。すでに設定済みの
キーのまま、キーボードが独立したプレイヤーになります。1 台の本体を 2 人で使い、片方がキーボード
という大会のローカル対戦を想定した機能です。

ゲームはキーボードと 1 台目のコントローラーに同じプレイヤー番号を与えるため、ローカル対戦では
同じキャラクターを操作してしまいます。ここでサイドを選ぶと、動くのは**コントローラー**の側です。
キーボードは設定済みのキーのまま、何も変わりません。

キー設定を書き換えることはありません。また、ゲームが持つ 2 つのキーボードプレイヤー間でキーボード
を移動することもしません — どちらも試して、どちらも何かを壊しました。ゲーム側のオプションで
2 人目のキーボードプレイヤーを設定している場合、そのキーはコントローラー側で反応します。
*Keyboard Player Number* を 1 にすると無効になります。

*Hold the side during a match* は、ローカル対戦中に両サイドのポート割り当てを毎フレーム書き込み、
選んだサイドを保ちます。オフにすると、どちらに入るかはゲームが決めます。

オンラインでは何も行いません。

### パレット

どちらのキャラクターにも任意の色をリアルタイムで適用できます。色は画面のサイドではなくキャラクター
に紐づくため、サイドが入れ替わっても一緒に移動し、各キャラクターが着ている色は記憶されて次回自動的
に適用されます。

パレットは通常の `.pal` ファイルとして `UNI2-IM\Palettes\<キャラクター>` に保存されます。ゲーム
自身の形式で、Hantei-kun も同じ形式を書き出します。名前・作者・説明を保持し、エフェクトの色も
パレットの一部として一緒に保存・読み込みされます。キャラクターのパレット一覧にある **Default** を
選ぶと、ゲーム本来の色に戻ります。

オンラインでは、自分のパレットは Steam 経由でゲームと並行して相手へ送られます。**See the other
player's colours** は相手サイドの扱いを 1 つのスイッチで決めます。オンなら届いたパレットを読んで
着せるため相手が選んだ色が見え、オフなら受信パケットを破棄して相手サイドはゲーム本来のままになり
ます。自分のパレットはどちらでも送信され、MOD を入れていない相手には何も見えません。

**Palette Nativa** は別の機能で、ゲーム自身のカラーカスタマイズをオーバーレイから操作します。
パーツごとにキャラクターの既定パレットから色を組み立てるため任意の色にはできませんが、ゲームが保存
するので MOD の有無にかかわらず **すべての** 対戦相手に見えます。

### Player Card

ゲームが対戦相手へ公開するカードを編集します。4 層のプレートとプレートタイトルです。タイトルは
自由入力で、ショップの 3 単語は選択 UI の状態にすぎないため、書いた文言がそのまま保存され相手に
届きます。

### BGM セレクター

MOD メニューの **Music** から開く専用ウィンドウ。

音楽が鳴る画面はすべてゲーム内の同じ入口を通るため、どの画面にも別の曲を割り当てられる。キャラ
の戦闘テーマ、キャラクター選択、VS 画面、メニュー。ゲーム本体は対戦テーマを 3 つ持つが、これは
その考え方から上限を外したもの。

**Soundpacks** は 1 つのゲームのサウンドトラックをまとめて導入し、全画面をそこに向ける。**Get
OST from French-Bread games** は手持ちのゲームから直接読み出す。`UNIclr.exe` または `UNIst.exe`、
`MBTL.exe`、`MBAA.exe` があるフォルダーを指定すると、曲名とループ位置つきで導入される。ダウン
ロードは一切なく、音声は MOD に同梱されない。同じゲームで二度実行しても、追加分が重複せず置き
換わる。Export と Import は自分のパックを 1 つの zip でやり取りするので、友人も同じ構成をこの
手順なしで揃えられる。

**Browse** は再生可能な全曲の一覧。検索と出典フィルターがあり、Play で 1 曲を再生して保持する。
Stop を押すとゲームに音楽が戻る。**Randomizer** をオンにすると、ゲームが音楽を要求するたびに
この一覧から無作為に 1 曲が渡される。曲ごとにスイッチがあるので、切った曲は選ばれない。

**Rules** は手動版。この対戦で、このキャラで、あるいはこの画面の代わりに、この曲を鳴らす。ルール
も書き出しと読み込みができ、読み込みは既存のリストに追加される。

自分の音楽も使える。`UNI2-IM/Music` の下のフォルダーに `.ogg` を置けば、他と並んで一覧に出る。

### Performance

Config から開く専用ウィンドウです。エンジンのフレーム進行には特定可能な問題があります。ゲームは
メッセージポンプと描画を別スレッドで動かしており、毎フレーム、描画スレッドはポンプだけが応答できる
メッセージで待たされます。そのポンプは眠っています。さらに Windows は、ゲームがバックグラウンドに
ある間 1 ms のタイマー精度を取り上げます。これが alt+tab の後に残る症状です。

3 つの選択肢があり、それぞれに実際の代償が併記されています。プリセットは 2 つです。ウィンドウは
MOD が要求した値ではなく、デバイスから読み戻した **実際に有効な** 値を表示します。

**Metrics** タブが計測を行います。フレーム間隔とそのばらつき、目標値周辺の 0.25 ms 刻みヒスト
グラム、中央値では見えないカクつきを捉える 2 クラスター検出、Present がブロックする時間、そして
不具合報告にそのまま貼れる要約です。

以前のビルドは既定でゲームの体感を悪化させていました。全員に 2 枚目のバックバッファーを追加し、
高リフレッシュレートのモニターをフルスクリーンで 60 Hz へ落としていたためで、入力遅延を 1 フレーム
増やすだけで何の利点もありませんでした。現在は指示がない限りディスプレイ設定に触れません。

### POTATO MODE

60 フレームを維持できないマシンのための、Performance ウィンドウの **POTATO MODE** タブです。

本作は毎フレームを固定 1280x720 の 5 枚のレンダーターゲットへ描画してから画面へ拡大するため、
負担のほとんどはそこにあります。POTATO MODE はそのサイズを下げます。半分なら 5 つの全画面パス
すべてでピクセル数が 4 分の 1、4 分の 1 なら 16 分の 1 です。**どの段階でも背景は描画されます** —
絵が甘くなるだけで、空にはなりません。

| 段階 | 描画サイズ | 併せて |
|---|---|---|
| Off | ゲーム本体の Display 設定のまま | なし |
| Balanced | 960x540 | バックバッファのマルチサンプリング無効、フレームのハンドシェイク待機 |
| Potato | 480p / 360p / 240p / 144p | さらに Character Visual Improvements 無効 |

**割合ではなくサイズです。** ウィンドウが 720p でも 1440p でも 640x360 は 640x360 のままなので、
ウィンドウをいくら大きくしてもゲームが描くピクセル数は変わりません。Direct3D が引き伸ばして
埋めます。エンジンはこれまでどおり描画するため、位置がずれることはなく、絵が甘くなるだけです。

**Off は完全に off** で、MOD はサイズに一切触れず、ウィンドウはゲーム本体の Display 設定に戻ります。

ウィンドウ／ボーダーレスのみ。描画サイズは次にゲームがディスプレイを構築したときに反映されます。

*Character Visual Improvements* はゲーム本体の設定です。キャラクターシェーダーのフィルター
テクニックを選び、1 ピクセルあたり 1 回で済むパレット参照を 9 回行いますが、得られるのは元
テクセル 1 つ分（720p でおよそ画面 1 ピクセル）のぼかしだけです。無効化は非力な GPU で最も安価な
実効果があり、ゲームのオプション画面が同じ値を書き戻すため MOD 側で保持し続けます。

*バックバッファのマルチサンプリング* を失っても損はありません。Direct3D 9 のテクスチャは
マルチサンプリングできないため、本作の Antialias はスプライトの輪郭に届きません。内部解像度を
100% より上げることが、このエンジンで唯一可能なアンチエイリアスで、ini ではそれも許可しています。

**ここにあるものはシミュレーションに一切触れません。** 対戦内容は同じで、オンラインの相手にも
分かりません。

レンダーターゲットは一度しか作られないため、解像度の変更は再起動後、またはゲーム本体の映像設定を
何か触った後に反映されます。それ以外は即時です。タブは**実際に**適用されている値を表示し、要求が
通っていなければそう伝えます。おかしいと感じたら段階を Off に戻してください。MOD は書き換えた
バイトをすべて元に戻します。

### Improvements

同じウィンドウの **Improvements** タブ。POTATO MODE の逆で、ウィンドウより*大きく*描画してから
Direct3D が縮小して合わせるため、各エッジが複数回サンプリングされます。

| 段階 | 描画サイズ |
|---|---|
| Off | ゲーム本体の Display 設定のまま |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

**できることとできないことを明確に。** 本作はその手前でキャラクターと背景を固定 1280x720 の 5 枚の
レンダーターゲットへ描画しており、そこには手を加えないため、スプライトの精細度は上がりません。
良くなるのはバックバッファへ直接描かれるもの — HUD、メニュー、合成のエッジ、MOD のオーバーレイ —
だけです。内部解像度ではなくスーパーサンプリングです。

負荷はサイズに比例します（4K は 720p の 9 倍）。**POTATO MODE が Off のときだけ表示されます。**
ウィンドウ／ボーダーレスのみ。

### Memory debug

既定では無効です。`[Debug] MemoryDebug = 1` を設定し **Ctrl+F1** で開きます。MOD の他の部分を
作るために使った生の表示と検索ツールです。

## 今後の予定

- **ロビーでのパレット表示**。対戦中だけでなく。

## ini ファイル

キーと既定値は上の英語版 [The ini file](#the-ini-file) と同じです。記載のないキーは既定値が使われ
るため、変更しない項目は削除して構いませんし、ファイルごと削除してやり直すこともできます。

## クレジット

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) — 設計の参考
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) — HA6 / CG / PAL 形式の基準
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) — MOD 制作の資料
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

### スペシャルサンクス

- Pescador Cearense
- Eon
- Listentothebirds - Rafael
- Willyofruit
- Sky Leite
- Excel
- ZateFGC
- Yorezordd (Velho fudido)
- Thiago
- Tanasinn [AZ]
- Licensed Grappler
- Anklegator
