# Player Control

A window of its own. It shows what both sides are inputting, live, as a numpad stick and four button
lights. You can point your own pad or keyboard at either character, hold a direction or a button on
the dummy, tap one for a few frames, or run a written script per side.

It also measures input lag: the wall clock from a physical press to that character's own input field
changing. The keyboard and every pad are sampled about a thousand times a second on a thread of
their own, rather than read from the game's once-a-frame poll, which would quantise every answer to
16.7 ms and measure nothing. Both of the pad APIs the game uses are covered.
