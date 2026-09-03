# Hitbox viewer

**F2**. Draws every box the engine has, for characters **and** projectiles, in the game's own
colours. Decoration is filtered out by the engine's own `_Exist_NoHantei` flag rather than guessed
at, so what you see is what the game reads.

There is no box for a throw, for the D Shield, or for proximity guard - the game does not have one.
A throw's catch region is an ordinary attack box on a frame carrying a throw attribute.
