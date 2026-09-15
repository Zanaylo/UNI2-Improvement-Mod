# Replacing game files

The game keeps almost everything in the `d` folder: a hundred or so files with scrambled names that
hold every sprite, sound, script and table. You cannot edit anything in there, and you do not need to.

**Put a file in `UNI2-IM\Mods` under the path the game knows it by, and the game reads your file
instead.** The game only reads from the `d` archive when `Mods` does not have the file.

## The path is the whole trick

Use the path the game asks for, from its own root, with the same folders:

```
UNI2-IM\Mods\data\chr002\chr002.ha6
UNI2-IM\Mods\se\normal_se\SE000.wav
UNI2-IM\Mods\grpdat\Announce\round_d00.pat
UNI2-IM\Mods\bg\bg028\bg.fbx.bin
```

Nothing is copied, nothing is packed, and the game folder is never touched. `d` stays as Steam
installed it, so a verify or an update finds nothing to repair, and your files survive both.

Case does not matter, and `/` and `\` are the same.

## It is live

Drop a file in while the game is running and the mod picks it up about half a second later. The game
only sees it the **next time it opens that file**, though. For most things that means the next match,
the next stage load or the next screen. Battle tables and the stage list are read once at startup and
need a restart.

Delete the file and the game gets its own file back the same way.

## What it covers

Anything the game opens by name, which is everything: character data and scripts, sound effects and
voices, music, stage models, `.pat` screen art, fonts, tables. The Voices, Stages and BGM tabs in the
overlay all write into `Mods` themselves. They are this same feature with a menu on top.

The overlay shows how many files are being replaced. "N file(s) are read before the d archive" means
it is working.

## Mods somebody else made

A mod is a **folder**. **Install a zip...** in the Mods window unpacks one for you, or you can put the
folder here yourself:

```
UNI2-IM\Packs\<the mod>\
```

Inside, it uses the game's own paths. For example, a mod that replaces Hyde's voice holds
`se\battle_se\chr000\` and nothing else. Open **Mods** in the overlay and it is in the list, with a
switch. No restart, no install step, and nothing is copied over the game.

Switch one off and the game gets its own files back the next time it opens them (the next match, the
next screen). Delete the folder and it disappears from the list about half a second later.

### Order

When two mods carry the same file, **the one higher in the list wins**. That only applies to that
file: the rest of the lower mod still works. Up and Down move mods in the list, and a row tells you
when it is losing files and to which mod.

**Your own files** is pinned at the top of the list. That row is the `UNI2-IM\Mods` folder from the
top of this page. It is always on and always first, and it is the place to test a change without
unpacking anyone's folder. Below it come the mods, in your order, and last the game's own `d` archive.
A loaded [game patch](Patches) sits above all of them.

### Two mods, one stage

A stage is more than files. It has to be registered in the game's stage list, and the game reads that
list once at startup. So a mod with a stage gets an **Install as a stage** button instead of using the
switch, and installing uses **the next free stage number**. Two mods made for the same number no
longer conflict: the second one goes to another number and both are playable. The row warns you before
you press the button.

### Making one

Use the layout above and put your files at the paths the game knows them by. Then add a `mod.ini` so
the mod shows a name instead of its folder name:

```ini
[Mod]
Name = Hyde speaks UNI
Author = you
Version = 1.0
Note = His UNI[st] voice, all 96 lines.
```

That is the whole format. A folder without `mod.ini` still works and is listed under its folder name.

**What you share and what you keep.** The folder is the mod: send it to someone, they drop it in
`Packs` and have what you have. Your switch and your order are saved in your own ini and are not part
of the mod.

**Online.** Nothing here is turned off when you go online, and art and sound change nothing anyone
else sees. But a mod with `data\` or `script\` files changes the simulation, and the other player will
desync. Read [A note on online play](Online-play).

## A patch beats it

An applied [patch](Patches) wins for the files it carries. A patch is a whole data version, so the game
checks it first. Anything it does not carry falls through to `Mods` and then to `d`.

## Online

**Nothing here is turned off when you go online.** A patch is, but this is not. Art, sound and music
are yours alone and change nothing anyone else sees. But **anything under `Mods\data` or `Mods\script`
changes the simulation**. The other player will desync, and the only fix is to move the file out and
restart. Read [A note on online play](Online-play).

## If it is not working

- Compare your path with the one in the log. `UNI2-IM\Logs` records every file the mod replaces and,
  for themes and sounds, every one it could not.
- A file in a folder that does not match the game's own layout is indexed but never used. There is no
  error, because the game simply never asks for it.
- If you are not sure of a file's name, the Voices and Stages tabs can show you the real names. The
  mod reads the `d` index itself.
