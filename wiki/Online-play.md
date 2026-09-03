# A note on online play

**Nothing in this mod is built to give anyone an advantage online, and nothing in it does.**

Every training tool that can alter what the simulation does - frame stepping, freezing, driving a
character by hand, the dummy scripts - is hard-gated to offline modes. The gate is the game's own
peer-to-peer traffic: while the game has sent a packet to an opponent in the last three seconds,
those tools refuse to run. The game uses GGPO rollback netcode, and anything that touches simulation
state during a match desynchronises it.

What does run online is cosmetic and read-only: the custom palettes, which travel beside the game
over Steam rather than through the netcode and cannot affect the match, and the performance options,
which only change how the frame reaches your monitor.

**The [patch selector](Patches) is the exception, and it is offline only.** A patch changes what the game
simulates. Its move tables and system constants are read once when the game starts and stay for
the whole session, so if you booted into a patch you are still on it online, whatever menu you
are in. It only works against an opponent who picked the same patch. **Do not use it in ranked,
or in any other match against somebody who did not - it will desync.** Restart on the installed
game before playing anybody.

If you find something here that gives an edge in a real match, that is a bug. Report it.

---
