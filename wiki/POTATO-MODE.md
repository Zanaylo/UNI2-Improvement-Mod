# POTATO MODE

The **POTATO MODE** tab of the Performance window, for a machine that cannot hold 60.

**The stage still draws at every level, and none of this changes the size of the picture.**

| Level | Draws at | Also |
|---|---|---|
| Off | the game's own Display option | nothing |
| Balanced | 960x540 | back buffer multisampling off, frame handshake waited on instead of the clock |
| Potato | **480p, 360p, 240p or 144p** | and Character Visual Improvements off |

Everything the mod draws keeps its normal size: the overlay, the frame meter, the hitboxes, the origin
cross. None of it gets stretched.

Pick Potato and a second row appears with the four sizes: `854x480`, `640x360`, `426x240`, `256x144`.
It only shows while Potato is the level, because a size option that does nothing looks broken.

**The size is a fixed size, not a fraction of your window.** 640x360 stays 640x360 whether the window
is 720p or 1440p. Make the window as big as you like: the game still draws that many pixels and
Direct3D stretches the result. The engine draws exactly as it always does, so nothing ends up in the
wrong place. The picture just looks soft, like a low resolution on a flat panel. 640x360 in a 720p
window is a quarter of the pixels to blend, scale and send out. In a 1440p window it is a sixteenth.

**Off means off.** The mod stops changing the size and the window goes back to the game's own
Option → Display.

**In exclusive fullscreen the size is rounded up.** A fullscreen back buffer must use a display mode
your adapter really has, and no modern monitor has `426x240`. So the mod asks the adapter and uses the
smallest listed mode that fits. Your monitor changes mode, and a 4:3 monitor will letterbox or
stretch. The tab shows what it picked. It only ever rounds *down* from where you started, which is
why **Improvements** stays windowed and borderless only.

The drawing size takes effect the next time the game builds its display: restart the game, or change
any video option in the game's own menu. Everything else on the tab applies right away.

*Character Visual Improvements* is the game's own option. It selects the filtered techniques in the
character shader, which do nine palette lookups per character pixel instead of one. All that buys is a
blur one source texel wide, about one screen pixel at 720p. Turning it off is the cheapest real gain
on a weak card. The mod keeps it off, because the game's own options screen turns it back on.

*Back buffer multisampling* costs nothing to lose. A Direct3D 9 texture cannot be multisampled, and
the whole scene is drawn into textures, so the game's Antialias never reaches a sprite edge. The only
thing it can touch is the single quad the finished frame is drawn with, and its only edges are the
edges of the screen. Raising the internal resolution above 100% is the only anti-aliasing this engine
can use, and the ini still allows it.

**Nothing here reaches the simulation.** Same match, nobody online can tell, and none of it is a
training tool. It only changes how the frame is drawn.

The tab shows what is **actually** in effect: the engine's size, the drawing space, the largest render
target seen, and whether every patched site was verified. If a request did not take, it tells you.

If anything looks wrong, set the level back to Off. The mod puts back every byte it changed.
