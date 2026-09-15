# Patches

Play an older balance version of the game.

The **Patches** tab lists the installed game and every patch you added. **Use** picks one. It loads
the next time the game starts.

## Both sides, or neither

A patch changes what the game simulates, so both players must be on the same one. Read
[A note on online play](Online-play) first. In short: **an agreed player match is fine, ranked never
is.**

The patch **stays loaded when you go online**. That is what makes an agreed match possible: the
battle tables are read at startup, and a patch cannot be swapped in or out during a session anyway.
So it is up to you to know who you are playing. Against anyone on the installed game, it desyncs. The
tab says so in red whenever a patch is loaded.

## Adding one

Under **Add a patch**:

1. Type the name. Use the version number, like `1.05` or `1.12`. The engine's balance numbers follow
   that name. A name without a version leaves them alone.
2. **Pick a folder**: the one that holds the patch's `data` and `script`.

The patch is copied into `UNI2-IM\Patches` and indexed. Its row then shows how many files it has and
how many of the twenty-seven characters it covers. A character the patch does not carry uses the
installed build, and the row warns you about it.

## Using one

**Use** arms a patch. It is not live until the game reads its files, and that only happens at a fresh
start. The button next to the list sends the game back to its loading screen, so it reads the patch
like a normal launch, without closing. From training, it leaves the battle first.

**Name the patch a replay wants** makes the mod pick the patch a replay was recorded on when you play
it back.

## Balance rules

Some balance changes are numbers in the engine, not files. The tab lists them under the patch. **On**
means the mod holds that number at the patch's value. *The game ships it from 1.xx* means the
installed game already works that way and the mod holds nothing.

These turn off as soon as the game goes online.

## Where things are

```
UNI2-IM\Patches\        the patches themselves
UNI2-IM\patches.ini     their names, notes and release dates
```

Deleting a patch from the list deletes its folder. The game's own files are never touched. A patch is
an extra place the game reads files from, not an edit.
