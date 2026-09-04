# Player Control

Its own window.

Shows what both sides are inputting, live, as a numpad stick and four button lights. Point your pad
or keyboard at either character, hold a direction or a button on the dummy, tap one for a few
frames, or run a written script per side.

It also measures input lag: wall clock from a physical press to that character's input field
changing. The keyboard and every pad are sampled about a thousand times a second on their own
thread. Reading the game's once-a-frame poll instead would round every answer to 16.7 ms and measure
nothing. Both pad APIs the game uses are covered.
