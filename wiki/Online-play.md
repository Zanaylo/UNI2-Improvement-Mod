# A note on online play

**Nothing in this mod is made to give an advantage online, and nothing in it does.**

Every training tool that can change the simulation only works offline: frame stepping, freezing,
controlling a character by hand, the dummy scripts. The mod checks the game's own peer-to-peer
traffic. If the game sent a packet to an opponent in the last three seconds, those tools will not run.
The game uses GGPO rollback, and changing the simulation during a match causes a desync.

What still runs online only changes what you see: the custom palettes, which travel over Steam
outside the game and cannot reach the match, and the performance options, which only change how the
frame gets to your monitor.

**The [patch selector](Patches) is the exception, and both players need it.** A patch changes what
the game simulates. Its move tables and system values are read once at startup and stay for the whole
session. If you started the game on a patch, you are still on it online, in any menu, and the mod
cannot change that from the network menu.

This is on purpose: **two players who both run the mod and picked the same patch can play each
other**, and a player match is the place for it. **Never play ranked on a patch, and never play
someone who is not on the same patch. It will desync.** If you are not sure who you are about to play,
restart on the installed game first.

If you find anything here that gives an edge in a real match, that is a bug. Report it.

## Watching someone's match

Players with the mod can watch each other's online matches without joining the room. Open the
Netplay window and go to **Spectate**.

- **To be watched**, tick **Allow spectators**. Your Steam friends with the mod see you in their list.
  To let anyone else watch, give them **your code**. Up to 24 people can watch.
- **To watch**, pick a friend from the list or type their code, then press **Watch**. You join at the
  start of their next match. You cannot be in a room of your own at the same time.
- **Kick** removes a viewer. Someone you kicked has to be let in by you before they can watch again.

A viewer only watches. Nothing they do reaches the match. A viewer who fails to connect makes the
players wait at most 5 seconds, then the match starts without them.

The viewer sees the host's stage. If it is a custom stage the viewer does not have, the viewer sees
stage 1. The viewer's game loads the same patch the host is playing, and skips the match if that patch
is not installed. When the match ends, the viewer's game shows the result and the replay menu, returns
to the main menu, and joins the host's next match on its own.

Viewers play a little behind the players. Their game keeps a buffer of the players' inputs, so a slow
connection or a slow loading screen means watching further behind, not freezing. Viewers never cause
rollbacks and never slow the players down. The host sends them fewer, larger packets to keep upload
low, and a viewer who falls too far behind is dropped and joins the next match.

This has not been tested in a real match yet.
