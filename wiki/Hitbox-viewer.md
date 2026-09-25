# Hitbox viewer

Press **F2**, or set **Hitbox Display** on the training menu's *Improvement Mod* page, the one after
*Training display*. On that line, the menu's **Open menu** button opens a guide to what every box means.

It draws every box the engine has, for characters and projectiles, in the game's own colours.
Decoration is hidden with the engine's own `_Exist_NoHantei` flag, not by guessing, so what you see is
what the game uses.

Throws, the D Shield and proximity guard have no box, because the game has none. A throw grabs with a
normal attack box, on a frame that has a throw attribute.

## Screens that are not 16:9

The game always draws a 16:9 picture and puts black bars around it when the window is a different
shape. The boxes follow that picture, so they stay on the characters at any resolution, windowed or
fullscreen, and a box that runs past the edge of the picture is cut there instead of being drawn on
the bars.

If boxes ever sit beside the characters instead of on them, say your resolution and whether the game is
fullscreen or windowed, and see [Reporting a problem](Reporting-a-problem).
