# Improvements

The **Improvements** tab of the Performance window. POTATO MODE the other way round: the frame is
drawn *larger* than your window and Direct3D fits it back down, so every edge is sampled several
times over.

| Level | Draws at |
|---|---|
| Off | the game's own Display option |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

**What it does not do.** The game rasterises the characters and the stage into five render targets
of a fixed 1280x720 before any of this, and that is untouched. A sprite gains no detail. What it
cleans up is everything drawn straight into the back buffer: the HUD, the menus, the composite's
edges and the mod's overlay. It is supersampling, not a higher internal resolution.

It costs fill rate in proportion to the size, and 4K is nine times 720p, so it is for a machine with
headroom. **The tab is only offered while POTATO MODE is Off**, because the two set the same drawing
size from opposite ends.

**Sharpening** is on the same tab and is the more useful half. The game rasterises at 1280x720 and
the composite blows that up to your window with a linear filter, so the softness is in the upscale,
not in the art. This puts the edge contrast back. It is contrast adaptive, so it follows edges
instead of ringing them, and it draws over the finished frame from `Present`: it patches nothing,
reads none of the game's shaders, and takes effect as you move the slider. 40-60%% is the useful
range. On a real frame it raises mean edge gradient by about 47%%.

Windowed and borderless only. Like POTATO MODE it takes effect the next time the game builds its
display, so restart it or touch any video option in the game's own menu.
