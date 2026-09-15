# Player Control

Its own window.

It shows what both sides are pressing, live, as a numpad stick and four button lights. You can control
either character with your pad or keyboard, hold a direction or a button on the dummy, tap one for a
few frames, or run a written script for each side.

It also measures input lag: real time from a physical press until that character's input changes. The
keyboard and every pad are read about a thousand times a second on their own thread. Reading the
game's once-per-frame poll would round every result to 16.7 ms and measure nothing. Both pad APIs the
game uses are supported.
