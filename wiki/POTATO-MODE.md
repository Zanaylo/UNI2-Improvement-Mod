# POTATO MODE

The **POTATO MODE** tab of the Performance window, for a machine that cannot hold 60.

**The stage still draws at every level, and none of this touches the picture's size.**

| Level | Draws at | Also |
|---|---|---|
| Off | the game's own Display option | nothing |
| Balanced | 960x540 | back buffer multisampling off, the frame handshake waited on instead of the clock |
| Potato | **480p, 360p, 240p or 144p** | and Character Visual Improvements off |

Everything the mod draws - the overlay, the frame meter, the hitbox boxes and the origin cross -
keeps the size it has always had, instead of growing along with the stretch.

Pick Potato and a second row appears with the four sizes: `854x480`, `640x360`, `426x240`,
`256x144`. It is only offered while Potato is the level, because a size that does nothing because
another level is selected reads as broken.

**The size is a size, not a fraction of your window.** 640x360 stays 640x360 whether the window is
720p or 1440p, so you can make the window as large as you like and the game still only ever draws
that many pixels - Direct3D stretches the result to fill it. The engine draws exactly as it always
does, so nothing in it has to know and nothing can end up in the wrong place; the picture just gets
soft, the way a low resolution looks on a flat panel. 640x360 in a 720p window is a quarter of the
pixels to blend, scale and scan out; in a 1440p window it is a sixteenth.

**Off means off.** The mod stops touching the size entirely and the window goes back to whatever the
game's own Option -> Display says.

**In exclusive fullscreen the size is rounded up.** A fullscreen back buffer has to name a display
mode your adapter really has, and `426x240` is not one on any monitor made this century. So the mod
asks the adapter and takes the smallest listed mode that fits what you picked, which means your
monitor changes mode and a 4:3 one will letterbox or stretch. The tab tells you what it settled on.
It only ever rounds *down* from where you started: drawing larger than the screen in fullscreen
would just be running the monitor at 4K, so **Improvements** stays windowed and borderless only.

The drawing size takes effect the next time the game builds its display: restart it, or touch any
video option in its own menu. Everything else on the tab is immediate.

*Character Visual Improvements* is the game's own option. It selects the filter techniques in the
character shader, which resolve nine palette lookups per character pixel instead of one, and what it
buys is a blur one source texel wide - about one screen pixel at 720p. Turning it off is the
cheapest real win on a weak card, and the mod holds it off, because the game's own options screen
writes the same setting back.

*Back buffer multisampling* costs nothing to lose: a Direct3D 9 texture cannot be multisampled at
all, so the game's Antialias never reaches a sprite edge - the only thing it can touch is the one
quad the finished frame is drawn with, whose only edges are the edges of the screen. Raising the
internal resolution above 100% is the only anti-aliasing this engine can be given, and the ini still
allows it.

**Nothing here reaches the simulation.** The match is the same match, nobody online can tell, and
none of it is a training tool. It is only how the frame is drawn.

The game builds its render targets once, so a resolution change takes effect after a restart, or
after you touch any video option in the game's own menu. Everything else is immediate. The tab
reports what is **actually** in force - the engine's own size, the drawing space, the largest render
target seen, and whether every patched site verified - so when a request has not taken, it says so
rather than leaving you to guess.

If anything looks wrong, set the level back to Off. The mod puts every byte it changed back.
