UNI2 Improvement Mod 0.2.0 - online palette test build
======================================================

Training tools and custom palettes for UNDER NIGHT IN-BIRTH II Sys:Celes
(Steam, Ver.0.10.0).

This build writes a log, because it is still a test build and the log is what
says why something did not work. Logs are in UNI2-IM\Logs next to the game; they
are plain text and contain nothing personal beyond the Steam ID of whoever you
played.


INSTALL
-------

1. Close the game.
2. Copy dinput8.dll and UNI2_IM.ini next to uni2.exe, usually

     ...\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\

3. Start the game and press F1.

To uninstall, delete dinput8.dll. Nothing else is touched and no game file is
modified.


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
