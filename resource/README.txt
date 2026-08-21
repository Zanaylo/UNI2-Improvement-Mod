UNI2 Improvement Mod
====================

Training tools and custom palettes for UNDER NIGHT IN-BIRTH II Sys:Celes
(Steam, Ver.0.10.0).

When something does not work, set

    [Debug]
    Logging = 1

in UNI2-IM\UNI2_IM.ini and run the game once. The log lands in UNI2-IM\Logs
next to the game and says what the mod loaded, what it is running on, every hook
it installed and where, and anything that faulted. Nothing is written without
that setting. The logs are plain text and contain nothing personal beyond the
Steam ID of whoever you played.

That ini is created on first run and completes itself after that: any key or
section it is missing is added with its default on every launch, and nothing you
edited is changed. If the mod never loads at all there is no folder to look in -
you can create UNI2-IM\UNI2_IM.ini with just the two lines above and the mod
fills in the rest.


INSTALL
-------

1. Close the game.
2. Copy dinput8.dll and UNI2_IM.ini next to uni2.exe, usually

     ...\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\

3. Start the game and press F1.

To uninstall, delete dinput8.dll. Nothing else is touched and no game file is
modified.


LINUX AND STEAM DECK (PROTON)
-----------------------------

Install it exactly as on Windows first: dinput8.dll next to uni2.exe, nothing
else. Proton 9 and newer load a mod's own dinput8.dll by themselves, so on a
current Proton that is all there is to it.

Older Proton does not. Wine picks which dinput8.dll to load from a prefix
setting, not from the folder the file is in, so there the DLL is simply never
loaded. If no UNI2-IM folder appears next to the game after a run, use ONE of
these - never both at once, or the mod loads twice into one process:

  1. Rename dinput8.dll to d3d9.dll and leave it next to uni2.exe. Nothing else
     to set: Proton treats d3d9 as native on every version, because that is how
     DXVK is installed. This file carries both sets of entry points.

  2. Or keep the dinput8.dll name and set the game's Steam launch options to

       WINEDLLOVERRIDES="dinput8=n,b" %command%

     which is the same thing Proton 9 and newer already do for you.

On Linux the mod leaves the host's display and scheduling tuning alone by
itself. [Compat] WineSafeMode = 0 in the ini takes it back.

Do not use the d3d9.dll name on Windows: RivaTuner refuses to hook a Direct3D
runtime outside a system folder, so that name and RTSS cannot both work.


RIVATUNER, MSI AFTERBURNER AND OTHER OVERLAYS
---------------------------------------------

Two overlays means two hook engines on the same Direct3D functions. The mod now
installs itself at the end of whatever chain is already on a function instead of
writing over it, which is what the RTSS author asks third parties to do, so both
overlays coexist and load order does not matter.

If the mod's overlay still never appears, or the game closes at startup with
RTSS running, turn logging on as above and the log names the hook and says what
happened to it. Either of these fixes it:

  - RTSS, Settings / General / Injection properties: tick
    "Use Microsoft Detours API hooking".
  - RTSS, this game's profile: set Application detection level to None. RTSS
    then leaves the game alone entirely, its own overlay included.


KEYS
----

  F1  overlay      F2  hitbox viewer     F3  frame meter
  F5  pause        F6  step one frame

**Do not press F5 in an online match.** The freeze stops the game's own tick and
would desync the match. Everything else only reads or paints and is safe.


WHAT IS NEW SINCE 0.1.0
-----------------------

  - **Online lock.** The training tools - pause, frame step, the frame meter,
    the dummy recorder and scripts, player control - now switch themselves off
    during a netplay match, and the hitbox viewer with them. Custom palettes
    keep working online on purpose: painting a palette texture never touches the
    simulation.

    The lock is the game's own peer traffic, which is the one thing no offline
    mode does. If you ever see the tools disabled in a LOCAL match, that is a
    bug and worth reporting immediately - the mod window's status line and the
    log both say "online" or "offline" outright.

  - **Mirror matches work.** 0.1.0 could not describe Mika against Mika at all
    and did nothing; the palette now says which SIDE it is for, so a mirror is
    ordinary.

  - **Palette ownership reworked.** Every palette texture the game creates is
    now tracked with an owner, one character can never hold two, and effects are
    told apart from fighters.

  - **Effect painting is off.** 0.1.0 also painted the shared effect texture,
    which turned other characters' effects into flat green blobs online - a
    character's palette is mostly padding and the padding is bright green.

  - Programmable dummy scripts, a player control window, and a lot of frame
    meter work. None of it runs online.


THE TEST: SHARING A PALETTE
---------------------------

Both players need this build. Then, each on your own machine:

1. Press F1, open Custom palettes, press "Load Palettes" once. That creates
   UNI2-IM\Palettes\<character>\ for every character.

2. Put a palette in the folder of the character YOU are going to play. Either
   drop a .pal in it, or open the editor in a training match, change a colour so
   the difference is obvious, and Save.

3. In training, pick that palette from the dropdown for your side and check that
   your character actually changes colour. **If it does not work offline it will
   not work online** - stop here and say so.

4. Tick "See online palettes".

5. Play an online match against each other. Casual is fine.

What should happen: about two seconds into the round, your opponent's character
turns the colours THEY chose, and yours turns the colours you chose on their
screen.

Beside the checkbox is a status line - "Steam ready, sent 1, received 1". That
is the thing to read, and a photo of it from both sides is the most useful
result. "no peer yet" means the mod never saw who you are playing.

Afterwards, both of you send the newest file from UNI2-IM\Logs. The interesting
lines start with SteamNetwork: and PaletteShare:.


KNOWN ISSUE IN THIS BUILD
-------------------------

**Online, the palette can land on the wrong character.** The sharing itself is
solid - the right palette arrives, on the right side, and the log proves it -
but which of the game's palette textures belongs to which side is worked out
from the order the game creates them, and that order is not guaranteed to be the
same on both machines in a netplay match. When it is wrong, your opponent wears
your colours or a character turns a flat colour.

Offline this is correct and has been played a lot. If you hit it online, a
screenshot of the mod's "Palette map" section next to the characters is exactly
what is needed to fix it.


WHAT IT DOES NOT DO
-------------------

  - It does not send anything through the game's netcode. Palettes travel beside
    the game over Steam, on a channel the game does not read, so they cannot
    affect the match. A player without the mod sees nothing at all.
  - It does not save what the other player sent. Their palette is painted for
    that match and forgotten.
  - It does not override your own choice. If you picked a palette for your
    character, that is what you see.
  - It does not modify, patch or replace any game file.


IF SOMETHING GOES WRONG
-----------------------

Delete dinput8.dll and the game is exactly as it was.

Worth reporting: what you were doing, which characters, and the status line or a
screenshot of the mod window.
