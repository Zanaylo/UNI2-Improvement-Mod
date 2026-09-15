# Improvements

The **Improvements** tab of the Performance window. It is the opposite of POTATO MODE: the game draws
the frame *larger* than your window and Direct3D scales it down, so every edge is sampled several
times.

| Level | Draws at |
|---|---|
| Off | the game's own Display option |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

**What it does not do.** Before any of this, the game draws the characters and the stage into five
render targets with a fixed size of 1280x720, and that does not change. Sprites get no extra detail.
What gets cleaner is everything drawn straight to the screen: the HUD, the menus, the edges of the
final image and the mod's overlay. This is supersampling, not a higher internal resolution.

The cost grows with the size, and 4K is nine times 720p, so use it on a machine with power to spare.
**The tab only shows while POTATO MODE is Off**, because both set the same drawing size from opposite
ends.

**Sharpening** is on the same tab, and it is the more useful part. The game draws at 1280x720 and
stretches that to your window with a linear filter, so the softness comes from the upscale, not from
the art. Sharpening brings the edge contrast back. It is contrast adaptive, so it follows edges
instead of putting halos around them. It draws over the finished frame from `Present`, changes
nothing in the game, reads none of the game's shaders, and updates as you move the slider. 40-60% is
the useful range. On a real frame it raises the mean edge gradient by about 47%.

Windowed and borderless only. Like POTATO MODE, it takes effect the next time the game builds its
display, so restart the game or change any video option in the game's own menu.
