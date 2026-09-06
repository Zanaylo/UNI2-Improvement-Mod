# A note on online play

**Nothing in this mod is built to give an advantage online, and nothing in it does.**

Every training tool that can change what the simulation does is locked to offline modes: frame
stepping, freezing, driving a character by hand, the dummy scripts. The lock is the game's own
peer-to-peer traffic. If the game has sent a packet to an opponent in the last three seconds, those
tools refuse to run. The game uses GGPO rollback, and touching simulation state during a match
desyncs it.

What still runs online is cosmetic and read-only: the custom palettes, which travel beside the game
over Steam and cannot reach the match, and the performance options, which only change how the frame
gets to your monitor.

**The [patch selector](Patches) is the exception, and it needs both players.** A patch changes what
the game simulates. Its move tables and system constants are read once at startup and stay for the
whole session, so if you booted into a patch you are still on it online, whatever menu you are in,
and nothing the mod can do at the network menu changes that.

That is deliberate: **two people who both run the mod and both picked the same patch can play each
other**, and a player match is where that belongs. **Never ranked, and never against someone who is
not on the same patch — it will desync.** If you are not sure who you are about to play, restart on
the installed game first.

If you find something here that gives an edge in a real match, that is a bug. Report it.
