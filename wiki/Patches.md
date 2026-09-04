# Patches

Play an older balance version of the game.

The **Patches** tab lists the installed game and every patch you added. **Use** picks one. It loads
on the next start.

## Offline only

A patch changes what the game simulates, so both players have to be on the same one. Read
[A note on online play](Online-play) first. Short version: **do not go online on a patch.** Restart
on the installed game.

Going online unloads the patch's files by itself, but the battle tables the game read at startup are
still the patch's. Only a restart clears those. The tab tells you which state you are in, in red
when it matters.

## Adding one

Under **Add a patch**:

1. Type the name. Use the version number, `1.05`, `1.12`. The engine's balance numbers follow that
   name; a name with no version in it leaves them alone.
2. **Pick a folder** — the one holding the patch's `data` and `script`.

It is copied into `UNI2-IM\Patches` and indexed. The row then shows how many files it has and how
many of the twenty-seven characters it covers. A character it does not carry falls back to the
installed build, and the row warns about it.

## Using one

**Use** arms a patch. It is not live until the game reads its files, which only happens at a fresh
start. The button beside the list sends the game back to its loading screen so it reads the patch the
way a launch would, without closing. From training it leaves the battle first.

**Name the patch a replay wants** makes the mod pick the patch a replay was recorded on when you
play it back.

## Balance rules

Some balance changes are numbers in the engine, not files. The tab lists them under the patch. **On**
means the mod is holding that number at the patch's value. *The game ships it from 1.xx* means the
installed game already behaves that way and nothing is held.

These come off the moment the game goes online.

## Where things are

```
UNI2-IM\Patches\        the patches themselves
UNI2-IM\patches.ini     their names, notes and release dates
```

Deleting a patch from the list removes its folder. Nothing in the game's own files is touched at any
point. A patch is a search path the game reads, not an edit.
