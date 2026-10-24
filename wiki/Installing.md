# Installing

Extract the release zip next to `uni2.exe`:

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

It has two files. `dinput8.dll` is the mod. `UNI2IMUpdater.exe` installs newer versions, and the
mod works without it.

Open `UNI2IMUpdater.exe` yourself and it checks GitHub. With no mod in the folder it offers to
download and install it; with an older version it offers the update; with the latest it says so
and does nothing. Close the game first. If it is not next to `uni2.exe`, it asks for the game
folder. The mod also runs it for you when you update from the overlay.

Press **F1** in game to open the overlay. To uninstall, delete both files.

The first time the mod runs, it writes its settings with the default values, one file per part of the
mod in `UNI2-IM\Config`, next to the DLL. After that they repair themselves: on every run, any missing
key or section is added with its default, and nothing you edited is touched. So a new version adds its
settings to your files, and a file you cut down to two lines gets filled back in. Delete a file to put
that part back to its defaults. A `UNI2_IM.ini` from an older version is split into these files on the
first start and kept as `UNI2_IM.ini.bak`. Every key and file is listed on [The ini file](The-ini-file).

The frame meter uses the game's own panel art and font. On the first run the mod copies them from the
game's `d` archive into `UNI2-IM\Assets`, so you do not extract anything by hand and the download has
no game data. Delete the folder and it is rebuilt. If the archive cannot be read, the meter still
works with flat colours.

To chain-load another `dinput8.dll` wrapper, put its full path in `[Mod] DinputDllWrapper`.

## When UNI2 updates

The mod only works with the game version it was made for. When Steam updates UNI2, the mod pauses
itself on the next start: a notice opens, nothing in the game is changed, and the mod only looks for
its own update, again every 15 minutes while the game is open. When the fixed version is out it
offers it, and installs it once you press **Update**. `[Compat] AllowUnsupportedGame = 1` in
`Config\Mod.ini` runs the mod anyway, at your own risk.

## Linux and Steam Deck (Proton)

Copy `dinput8.dll` next to `uni2.exe`, the same as on Windows. Then do one extra step that Windows
does not need: tell Wine to load it.

1. In your Steam library, right click **UNDER NIGHT IN-BIRTH II Sys:Celes** → **Properties**.
2. Under **General**, in **Launch options**, paste exactly this:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

3. Start the game and press **F1**.

That is all. You do not rename or copy anything else.

**Why you need it.** Wine chooses which `dinput8.dll` to load from an override set for the prefix, not
from the folder the file is in. Without that line the DLL sits next to `uni2.exe` and never loads.
`n,b` means native first, then built-in: the mod's copy loads, and Wine's own `dinput8` still handles
everything the mod passes on. That is why controller input keeps working.

Proton 9 and newer already do this for a mod's own `dinput8.dll`, so there the line changes nothing
and is safe to keep. Older Proton does not, and will not load the mod without it.

On Linux the mod turns on **compatibility safe mode** by itself: no fullscreen refresh rewriting, no
power throttling opt-out, no `Sleep` substitution. Those three are tuning for Windows, and on Linux
DXVK and the kernel already do that job better. Set `[Compat] WineSafeMode = 0` to turn them back on,
or to check whether one of them is causing a problem.

If nothing happens at all, look for a `UNI2-IM` folder next to `uni2.exe`. If there is no folder, the
DLL never loaded, so the problem is the step above, not the mod. To get a log from a machine where it
does load, create `UNI2-IM/UNI2_IM.ini` by hand with only these two lines and start the game once. The
mod merges it into its settings files and fills in the rest:

```ini
[Debug]
Logging = 1
```

## RivaTuner, MSI Afterburner and other overlays

Two overlays in one game means two programs hooking the same Direct3D functions, and often one of
them writes over the other. RTSS checks that its own hook is still in place and puts it back, which
removes the other program's hook. Then the mod's overlay draws for one frame and never again, or the
game crashes at startup.

The mod no longer writes over anybody. Each hook follows the hooks already installed on a function and
adds itself at the end, which is what the RTSS author asks other programs to do. Both overlays work
together and load order stops mattering. This is also why the Steam overlay works cleanly now.

If something still goes wrong, set `[Debug] Logging = 1` and run the game once. The log in
`UNI2-IM/Logs` names each hook and says what happened to it, and the Debug window shows the same thing
live. Two RTSS settings fix the rest:

- **Settings → General → Injection properties → "Use Microsoft Detours API hooking".** RTSS switches
  to a hooking method made to work alongside other programs.
- **The game's RTSS profile → Application detection level → None.** RTSS leaves the game alone
  completely, and its own overlay is gone too.
