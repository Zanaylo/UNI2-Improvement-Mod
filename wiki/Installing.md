# Installing

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
[The ini file](The-ini-file).

The frame meter draws with the game's own panel art and font. The mod lifts those files out of the
game's `d` archive into `UNI2-IM\Assets` on first run, so there is nothing to extract by hand and no
game data in the download. Delete the folder and it is rebuilt; if the archive cannot be read the
meter still works, drawn in flat colours.

To chain-load another `dinput8.dll` wrapper, put its full path in `[Mod] DinputDllWrapper`.

## Linux and Steam Deck (Proton)

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

## RivaTuner Statistics Server, MSI Afterburner and other overlays

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
