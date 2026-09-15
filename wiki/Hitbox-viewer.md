# Hitbox viewer

Press **F2**.

It draws every box the engine has, for characters and projectiles, in the game's own colours.
Decoration is hidden with the engine's own `_Exist_NoHantei` flag, not by guessing, so what you see is
what the game uses.

Throws, the D Shield and proximity guard have no box, because the game has none. A throw grabs with a
normal attack box, on a frame that has a throw attribute.
