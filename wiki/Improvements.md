# Improvements

The **Improvements** tab of the same window, and POTATO MODE the other way round: the frame is drawn
*larger* than your window and Direct3D fits it back down, so every edge is sampled several times over.

| Level | Draws at |
|---|---|
| Off | the game's own Display option |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

**Be clear about what it does and does not do.** The game rasterises the characters and the stage
into five render targets of a fixed 1280x720 before any of this, and that is untouched - a sprite
gains no detail from it. What it cleans up is everything drawn straight into the back buffer: the
HUD, the menus, the composite's edges, and the mod's own overlay. It is supersampling, not a higher
internal resolution.

It costs fill rate in proportion to the size - 4K is nine times 720p - so it is for a machine with
headroom, which is why it lives on its own tab. **The tab is only offered while POTATO MODE is Off**,
because the two settle the same drawing size from opposite ends.

**Sharpening** is on the same tab and is the more useful half. The game rasterises everything at
1280x720 and the composite blows that up to your window with a linear filter, so the softness you see
is in the upscale rather than in the art. This puts the edge contrast back - contrast adaptive, so it
follows edges instead of ringing them - and it is drawn over the finished frame from `Present`, which
means it patches nothing, reads none of the game's shaders and takes effect the moment you move the
slider. 40-60%% is the useful range. Measured on a real frame it raises mean edge gradient by about
47%%.

Windowed and borderless only, and like POTATO MODE it takes effect the next time the game builds its
display: restart it, or touch any video option in its own menu.
