# Stages

**Stages** on the main window, then **Open stages**. Two tabs:

- **Installed Stages** — the stages UNI2 hides from its own picker, then every stage this build
  has: the game's own and everything you have ported, each with its own colour. Only a port can be
  removed; **Default** puts a stage's colour back to what it started at.
- **Add stages** — take a stage out of another French-Bread game you own, or install a stage of
  your own from a folder.

Both write to `UNI2-IM\Mods\bg`. The game reads its stage list once, at startup, so anything you
change here shows up after a restart. The panel says so and offers the restart button.

## The hidden stages

UNI2 ships two stages it never offers:

- **煌朧の祭壇**, the altar. A finished stage with a name and a card.
- **The debug stage.** No background, just the grid.

Tick one and it joins the picker. Untick and it leaves.

The stage records carry flags that look like they hide these, but the picker is built from
`BgSelectList` in `BgList.txt`, and that is what the mod edits. Clearing the flags alone does
nothing, which is why an earlier build never made them appear.

## Porting a stage

**Get stages from French-Bread games**, then pick the folder holding `MBTL.exe`, `UNIclr.exe` or
`UNIst.exe`. You get that game's stage list with names and sizes. Press **Add**, name it, press
**Install**.

MBTL and UNI store a stage exactly the way UNI2 does: the same `fbxex` model, the same loose DDS
textures, the same 2D object layer. So nothing is converted. The files are read out of your own copy
and written next to the game as a stage of its own. Nothing is downloaded and nothing the game ships
is replaced.

Sizes: UNI stages are 4-30 MB, MBTL's are 17-98 MB.

**Names** are in English. UNI's stages are stages UNI2 also ships, so they use UNI2's own English
name. MBTL's are translated in the mod.

**Cards** come from the source game. UNI stages point at the card UNI2 already draws for them. MBTL
stages get theirs lifted out of MBTL's own picker sheet. Ports numbered past 47 have no free cell
left and borrow one.

**Remove** deletes the stage's files, its entry and its card.

**Every port has its own card.** The picker's sheet holds 48 cells and the game uses 0 to 27, so
only twenty are free - fewer than you can install. The mod makes those twenty carry whatever is on
screen: ports twenty apart share a cell, and because the picker shows about seven cards at a time
they can never be visible together, so the mod repaints the cells as the cursor moves. You see each
stage's own card and the game never knows.

It needs the cards on disk, which are written when a stage is installed, so **a stage added before
this needs adding again once**. If it cannot paint - the sheet in video memory, say - the Stages
panel says so and the picker looks as it did before.

## Colour

UNI2 adds a flat 0.10 to every background pixel on its way to the screen, in
`Shader/sh_bgspeculer.txt`. Its own stages are painted for that — decode one of the game's own
stage cards and no pixel in it sits below 26 of 255 — and DFCI's are not, so a DFCI port used to
arrive with its blacks lifted and read grey.

Each ported stage carries its own **Lift** and **Contrast**, on its row in the ported list.
They apply **live**: drag one while a match is running and the background changes under
you. Nothing is re-imported and nothing is restarted.

The game draws a background as `lift + texture * contrast`, so:

- **Lift** is a flat amount added to every pixel. **Raising it makes the stage brighter and
  flatter**; lowering it deepens the blacks. `0.10` is what UNI2 adds to its own stages and is where
  every stage but a DFCI port starts. `0.00` is what DFCI adds, which is nothing.
- **Contrast** multiplies the texture. `1.00` is the game's own.

A DFCI port starts at lift `0.00` and contrast `1.50` — the pair that puts a port back on top of
the same stage as DFCI drew it.

You should not have to touch either one: a DFCI port already starts where it belongs. They are there
for when a particular stage wants a nudge.

They are stored under `[StageColour]` in the ini as `StageNN=lift,contrast`, and are cleared when
the port is removed, so a stage number that gets reused does not inherit the last one's colour. The
game's own stages are not affected — they keep the 0.10 they were painted for.

The order is: a DFCI port's pair first, then the game's own; anything you drag yourself sits on
top.

**Bloom is off on a DFCI port.** DFCI has no background bloom pass at all, and UNI2's is tuned for
its own line-up, so a port used to carry a glow its source never had — with a green channel at
0.3 against 0.5 for red and blue, which is a colour cast no black level or contrast can cancel.

## Sprites a stage asks for and does not have

DFCI's Valkyria Chronicles (day) stage has eleven ember emitters, and three separate faults kept
them off the screen. All three are repaired on the way in:

- Seven of them name a sprite its own sheet does not contain. The mod points a missing name at the
  one sprite that unambiguously matches, and leaves it alone and writes to the log when none does.
- Every one of them is drawn at priority 0, which is the very back — behind the 3D stage. Every
  other DFCI stage uses 700. The mod raises a zero priority to 700, but **only** where the entry is
  not depth-tested; the ones that ask to interact with the background are meant to be back there.
- Its entries are numbered from zero and one number is used twice, which loses an emitter. The mod
  renumbers them in file order.

## A stage of your own

**Import a stage folder**, at the bottom of **Add stages**. The folder needs a stage's own files -
`bg.fbx.bin`, its textures, the `.pat` and `object.txt` if it has them - and a `stage.txt`.

You already have folders in that shape: **every stage the mod installs is one**, under
`UNI2-IM\Mods\bg\bgNNN`. The **Open** button on a stage's row opens it. So the round trip
is: open a stage's folder, copy it somewhere, edit it, and import the copy.

`stage.txt` is written by the mod beside every stage it installs. It carries the name the picker
should use and the stage's own numbers - scale, position, field of view, fog, bloom. Without it a
custom stage still installs, under the folder's name and with the mod's defaults, but it will
probably sit at the wrong scale, so keep it.

It is also what tells you which stage a folder called `bg052` is, in Explorer and in Blender - the
add-on names its collection from it.

### Making one brighter

A stage that reads too dark is the commonest thing to hit, and **the vertex colours are not where to
fix it**. The game draws a background as

```
lift + texture * vertex colour * contrast
```

and a vertex colour cannot go above 1 — Blender clamps it when the attribute is written, whatever
you type. **Contrast is that same multiply with the rest of the range in it.** Contrast 2.0 is a
vertex colour of 2.0, live, and no re-export.

So: put the stage on screen, open **Stages**, and drag **Contrast** on its row until it looks right.
Raise **Lift** only if the blacks need lifting too — it adds a flat amount to every pixel, so it
brightens and flattens together.

Nothing in `BgList.txt` reaches this. It has no brightness field.

### Two made for you

The mod ships two custom stages under `UNI2-IM\Custom`, to import the same way:

- **Golden Hall** - built from nothing, no game files involved. Five drawn textures and forty
  quads.
- **Quiet Park at Dusk** - the game's own Quiet Park, regraded to evening. A stage's lighting lives
  in its vertex colours, so recolouring those changes the whole thing without moving a triangle.

## MBAACC

Not offered. Its backgrounds are 2D sprite layers in a `bgmake` container that shares nothing with
UNI2's format, and UNI2 has no 2D stage to put them in. Its **music** does import, from the
[BGM selector](BGM-selector).

## Limits

- Stage numbers 28 to 89 are free, so that is 62 ports. Only 28 to 47 get their own card.
- Ports are yours alone. Do not expect them to work online.
- Hilda's UNI1 stage is not one of the hidden two. Stage 23 is French-Bread's own new Hilda stage.

## The model underneath

[Stage models](Stage-models) documents every file a stage is made of, the `fbxex` container the 3D
model lives in, and how to take that model out to Wavefront OBJ, edit it in Blender and put it back.
