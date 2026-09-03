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

If you find something here that gives an edge in a real match, that is a bug. Report it.

---
