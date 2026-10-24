# Improvements

The **Improvements** tab of the Performance window, also in the game's own menu under Option, Display,
Improvement Mod Display Settings. It is the opposite of POTATO MODE: the game is drawn bigger than
720p. It has two settings, and they do different jobs.

| Setting | What it sizes | What gets sharper |
|---|---|---|
| **Output resolution** | the finished picture sent to your screen | the stretch of the game's 720p picture, done by the upscale filter on the Shaders tab |
| **Render resolution** | the 3D stage | stage detail: small text, window frames and carvings |

The game draws its scene at 1280x720 and stretches it to the output resolution. The stage is 3D, so
the render resolution draws it with real detail, the way Melty Blood Type Lumina and BlazBlue do.
Characters, effects, menus and text are 720p art. With **Characters and text at this size too** (on by
default) they are drawn at the render resolution as well. Under it, **Characters** is Smooth (soft
edges) or Pixels (the art's own pixels, enlarged), and **Text** is Smooth (like the frame meter) or
Sharp (hard edges). Both apply right away. Art has no hidden detail, so menus and the HUD look as
smooth as before, not more detailed.

**Set both to your screen's resolution.** On a 4K screen: output 4K, render 4K. On a 1440p screen:
output 1440p, render 1440p.

## Output resolution

| Level | Picture size |
|---|---|
| Off | the game's own Display option |
| 1080p | 1920x1080 |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

Windowed and borderless only. In the ini it is `Supersample`.

## Render resolution

| Level | Stage drawn at |
|---|---|
| Off | 1280x720, the game's own |
| 1080p | 1920x1080 |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

In the ini it is `InternalResolution`.

**Characters and text at this size too** draws everything else at the render resolution as well. Off,
characters, effects and menus stay a 720p picture: where something covers the stage, that pixel comes
from the 720p picture, and with the upscale filter Off the mod uses bicubic for it. Measured on a 4K
screen: still 60 fps with no frame over 18 ms in training, about 2 MB more memory. Needs a restart. In
the ini it is `SpritesAtRenderResolution`.

It changes only what is drawn: gameplay, timing and what your opponent sees are untouched, and no
byte of the game is changed.

Measured on a 4K screen with a Radeon RX 7600, 4K output and 4K render: stage edge detail 1.58 at
Off and 2.54 at 4K on the same crop, menu text identical to Off, and no frame over 17 ms in the menu,
training or the training menu. 4K uses about 260 MB more video memory.

## Both settings

They need a restart. **The tab only shows while POTATO MODE is Off**, because both set the drawing size
from opposite ends, and the render resolution is off while POTATO MODE is on. **Everything off** on the
Shaders tab turns both off.

**Sharpening** is on the Shaders tab. It brings back the edge contrast a stretch softens. It is
contrast adaptive, so it follows edges instead of putting halos around them. 40-60% is the useful
range.
