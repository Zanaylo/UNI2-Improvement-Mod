# Replacing game files

The game keeps almost everything in the `d` folder — a hundred-odd files with scrambled names
holding every sprite, sound, script and table it owns. Nothing in there can be edited in place, and
nothing has to be.

**Put a file in `UNI2-IM\Mods` under the path the game knows it by, and the game reads yours
instead.** The `d` archive is only asked for what `Mods` does not answer.

## The path is the whole trick

Use the path the game asks for, from its own root, with the same folders:

```
UNI2-IM\Mods\data\chr002\chr002.ha6
UNI2-IM\Mods\se\normal_se\SE000.wav
UNI2-IM\Mods\grpdat\Announce\round_d00.pat
UNI2-IM\Mods\bg\bg028\bg.fbx.bin
```

Nothing is copied, nothing is packed, and the game folder itself is never touched — `d` stays as
Steam installed it, so a verify or an update finds nothing to repair and your files survive both.

Case does not matter, and `/` and `\` are the same.

## It is live

Drop a file in while the game is running and it is picked up about half a second later. The game
only sees it the **next time it opens that file**, though, which for most things means the next
match, the next stage load or the next screen. Battle tables and the stage list are read once at
startup and need a restart.

Deleting the file gives the game its own back the same way.

## What it covers

Anything the game opens by name, which is everything: character data and scripts, sound effects and
voices, music, stage models, `.pat` screen art, fonts, tables. The Voices, Stages and BGM tabs in
the overlay all write into `Mods` themselves — they are this feature with a front end.

The overlay says how many files are answering. "N file(s) are read before the d archive" means it is
armed.

## Mods somebody else made

A mod is a **folder**. **Install a zip...** in the Mods window unpacks one for you, or you can put
the folder here yourself:

```
UNI2-IM\Packs\<the mod>\
```

Inside, it has the game's own paths — a mod that replaces Hyde's voice holds
`se\battle_se\chr000\` and nothing else. Open **Mods** in the overlay and it is in the list, with a
switch. No restart, no install step, and nothing is copied over the game.

Switch one off and the game has its own back the next time it opens those files — the next match,
the next screen. Delete the folder and it is gone from the list about half a second later.

### Order

When two mods carry the same file, **the one higher in the list wins** — that file only; the rest of
the mod below still applies. Up and Down move them, and a row tells you when it is losing files and
to which mod.

The list has **Your own files** pinned at the top. That row is the `UNI2-IM\Mods` folder from the
top of this page: it is always on, always first, and it is the place to try a change without
unpacking anybody's folder. Below it come the mods, in your order, and last the game's own `d`
archive. A loaded [game patch](Patches) sits above the lot.

### Two mods, one stage

A stage is not just files — it has to be registered in the game's stage list, and that list is read
once when the game starts. So a mod carrying a stage gets an **Install as a stage** button instead
of working off the switch, and installing takes **the next free stage number**. Two mods built on
the same number therefore stop fighting: the second one lands somewhere else and both are playable.
The row warns you before you press it.

### Making one

Take the layout above and put your files at the paths the game knows them by. Then add a `mod.ini`
so it has a name instead of a folder name:

```ini
[Mod]
Name = Hyde speaks UNI
Author = you
Version = 1.0
Note = His UNI[st] voice, all 96 lines.
```

That is the whole format. A folder with no `mod.ini` still works — it is listed under its folder
name.

**What travels and what does not.** The folder is the mod: whoever you send it to drops it in
`Packs` and has what you have. Your switch and your ordering are yours, in your own ini, and do not
travel with it.

**Online.** Nothing here is unloaded when you go online, and art and sound change nothing anyone
else sees. A mod carrying `data\` or `script\` files is simulation, though, and the other player
will desync — read [A note on online play](Online-play).

## A patch beats it

An applied [patch](Patches) wins for files it carries. A patch is a whole data version, so it is
asked first; everything it does not carry falls through to `Mods` and then to `d`.

## Online

**Nothing here is unloaded when you go online.** A patch is; this is not. Art, sound and music are
yours alone and change nothing anyone else sees, but **anything under `Mods\data` or `Mods\script`
is simulation** — the other player will desync, and the only way to put it back is to move the file
out and restart. Read [A note on online play](Online-play).

## If it is not working

- Check the path against the one in the log — `UNI2-IM\Logs` records every file it answers and, for
  themes and sounds, every one it could not.
- A file inside a folder that does not match the game's own layout is indexed and never asked for.
  There is no error for that; the game simply never asks.
- Anything you are not sure of the name of: the mod reads the `d` index itself, so the Voices and
  Stages tabs can show you the real names.
