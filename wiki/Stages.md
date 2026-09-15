# Stages

Press **Stages** on the main window, then **Open stages**. There are two tabs:

- **Installed Stages**: the stages UNI2 hides from its own picker, then every stage this build
  has. That means the game's own stages and everything you have ported, each with its own colour.
  Only a port can be removed. **Default** puts a stage's colour back to where it started.
- **Add stages**: take a stage out of another French-Bread game you own, or install a stage of
  your own from a folder.

Both tabs write to `UNI2-IM\Mods\bg`. The game reads its stage list only once, at startup, so your
changes show up after a restart. The panel tells you this and offers a restart button.

## The hidden stages

UNI2 ships two stages it never offers:

- **煌朧の祭壇**, the altar. A finished stage with a name and a card.
- **The debug stage.** No background, just the grid.

Tick one and it joins the picker. Untick it and it leaves.

The picker is built from `BgSelectList` in `BgList.txt`, and that is what the mod edits. The stage
records also have flags that look like they hide these stages, but clearing them alone does nothing.

## Porting a stage

Press **Get stages from French-Bread games**, then pick the folder that holds `MBTL.exe`,
`UNIclr.exe` or `UNIst.exe`. You get that game's stage list with names and sizes. Press **Add**,
give it a name, then press **Install**.

MBTL and UNI store a stage the same way UNI2 does: the same `fbxex` model, the same loose DDS
textures, the same 2D object layer. Nothing is converted. The files are read from your own copy of
the game and written next to UNI2 as a new stage. Nothing is downloaded and nothing the game ships
is replaced.

Sizes: UNI stages are 4-30 MB, MBTL stages are 17-98 MB.

**Names** are in English. UNI's stages are also in UNI2, so they use UNI2's own English name. MBTL
stage names are translated by the mod.

**Cards** come from the source game. UNI stages use the card UNI2 already has for them. MBTL stages
get their card from MBTL's own picker sheet. Ports numbered past 47 have no free cell left and
borrow one.

**Remove** deletes the stage's files, its entry and its card.

**Every port shows its own card.** The picker's sheet has 48 cells and the game uses 0 to 27, so
only twenty are free. That is fewer than you can install. Ports twenty apart share a cell, but the
picker only shows about seven cards at a time, so they are never on screen together. The mod
repaints the cells as the cursor moves, so you always see the right card.

The cards are written to disk when a stage is installed, so **a stage added before this feature
must be added again once**. If the mod cannot paint the cards (for example, when the sheet is in
video memory), the Stages panel tells you and the picker looks like it did before.

## Colour

UNI2 adds a flat 0.10 to every background pixel, in `Shader/sh_bgspeculer.txt`. Its own stages are
painted with that in mind (no pixel on the game's own stage cards is below 26 of 255). DFCI stages
are not, so a DFCI port would show grey blacks.

Each ported stage has its own **Lift** and **Contrast** on its row in the ported list. They apply
**live**: drag one during a match and the background changes right away. Nothing is re-imported or
restarted.

The game draws a background as `lift + texture * contrast`:

- **Lift** is a flat amount added to every pixel. **Raising it makes the stage brighter and
  flatter.** Lowering it deepens the blacks. `0.10` is what UNI2 adds to its own stages and is the
  starting value for every stage except a DFCI port. `0.00` is what DFCI adds, which is nothing.
- **Contrast** multiplies the texture. `1.00` is the game's own value.

A DFCI port starts at lift `0.00` and contrast `1.50`. That pair makes the port look like the same
stage in DFCI.

You should not need to touch either one, because a DFCI port already starts at the right values.
They are there for when a stage needs a small nudge.

They are saved under `[StageColour]` in the ini as `StageNN=lift,contrast`. They are cleared when
the port is removed, so a reused stage number does not inherit the old colour. The game's own
stages are not affected and keep their 0.10.

The order is: a DFCI port's pair first, then the game's own. Anything you drag yourself goes on
top.

**Bloom is off on a DFCI port.** DFCI has no background bloom pass, and UNI2's bloom is tuned for
its own stages. On a port it added a glow the original never had, with a green tint (green at 0.3
against 0.5 for red and blue) that no lift or contrast setting can fix.

## Light strength

A BBTAG port draws its lamps, glows, flares and water sheets **added** instead of blended, the same
way BBTAG does. Added surfaces are bright, so every stage has its own **Light** setting next to Lift
and Contrast. A BBTAG port starts at `0.50` and every other stage at `1.00`. `0.00` turns the added
surfaces off and `2.00` doubles them.

It changes **only** the surfaces that add. The rest of the stage keeps the brightness from Lift and
Contrast. Use Light when a stage looks right but its lights are too bright or too dim. It is saved
as `StageNNGlow` under `[StageColour]` and is cleared by the same **Default** button.

## Size

**Size**, on the same row, sets how large the stage is drawn around the fight. The fighters do not
change, so a larger stage makes them look smaller against it. Use it when a port's scenery looks too
big or too small next to the characters. UNI2's own stages ship at `10`, and ports are converted to
match, so you should only need a small nudge.

Size changes all three axes together and keeps the stage's shape. For the loaded stage there is also
**Where the fight sits in this stage**, above the list. It opens the three axes and the position
separately. Use it when a stage needs to move rather than grow. Both are saved under
`[StagePlacement]` as `StageNNN=scale x,y,z,position x,y,z`, and **Reset** puts back the stage's
original values.

## Sprites a stage asks for and does not have

DFCI's Valkyria Chronicles (day) stage has eleven ember emitters, and three problems kept them off
the screen. The mod fixes all three when it installs the stage:

- Seven of them ask for a sprite that is not in the stage's sheet. The mod points the missing name
  at the one sprite that clearly matches. If none does, it leaves it alone and writes to the log.
- All of them are drawn at priority 0, the very back, behind the 3D stage. Every other DFCI stage
  uses 700. The mod raises a zero priority to 700, but **only** when the entry is not depth-tested.
  Entries that interact with the background are meant to stay at the back.
- Its entries are numbered from zero and one number is used twice, which loses an emitter. The mod
  renumbers them in file order.

## A stage of your own

Use **Import a stage folder**, at the bottom of **Add stages**. The folder needs the stage's own
files (`bg.fbx.bin`, its textures, and the `.pat` and `object.txt` if it has them) plus a
`stage.txt`.

You already have folders like this: **every stage the mod installs is one**, under
`UNI2-IM\Mods\bg\bgNNN`. The **Open** button on a stage's row opens it. So you can open a stage's
folder, copy it somewhere, edit it, and import the copy.

The mod writes `stage.txt` next to every stage it installs. It holds the name the picker uses and the
stage's own numbers: scale, position, field of view, fog and bloom. A custom stage without it still
installs, using the folder's name and the mod's defaults, but it will probably have the wrong scale.
Keep it.

### Editing stage.txt

`stage.txt` holds the values that would otherwise go in the stage's `Bg_` block of `BgList.txt`, so
nobody who installs your stage has to edit `BgList.txt`. The mod reads it every time the game starts.
Edit the copy in `UNI2-IM\Mods\bg\bgNNN` and restart.

```
Name = "My stage"
FOV = 43.0,
ShadowScale = 0.8,
ShadowLightType = 0,
ShadowLightStatus =
[
	{ Type=1, Position=0.0, PowerValue=7.0, Color=[0.0,0.0,0.0,0.8] },
],
```

A whole `Bg_090 = { ... }` block copied from `BgList.txt` works too.

- `Name` is the name in the picker.
- **A stage imported from a folder uses every value.**
- **A stage ported from another game** uses only the values the port needs: scale, position, field
  of view, fog, bloom, lights and shadows.
- **In `Mods\bg\bgNNN` for one of the game's own numbers**, it changes that stage. See
  [Replacing one of the game's stages](#replacing-one-of-the-games-stages).
- `StageW` is never below `4096`, the width of every UNI2 stage, so both players have the same walls.
- The mod picks the card, so `StageSelTex` is ignored. `DataFile`, `From` and `Source` are the mod's
  own and are ignored too. `Flow`, `Lamp`, `VertexAlpha` and `CharaTint` are read by the mod and never
  reach `BgList.txt`.

It also tells you which stage a folder like `bg052` is, in Explorer and in Blender. The add-on names
its collection from it.

### Making one brighter

A stage that looks too dark is the most common problem, and **the vertex colours are not the place
to fix it**. The game draws a background as

```
lift + texture * vertex colour * contrast
```

and a vertex colour cannot go above 1. Blender clamps it when the attribute is written, no matter
what you type. **Contrast is the same multiply, without that limit.** Contrast 2.0 equals a vertex
colour of 2.0, live, with no re-export.

So put the stage on screen, open **Stages**, and drag **Contrast** on its row until it looks right.
Raise **Lift** only if the blacks need lifting too. It adds a flat amount to every pixel, so it
makes the stage brighter and flatter at the same time.

`BgList.txt` cannot help here. It has no brightness field.

### Two made for you

The mod ships two custom stages under `UNI2-IM\Custom`. Import them the same way:

- **Golden Hall**: built from scratch, with no game files. Five drawn textures and forty quads.
- **Quiet Park at Dusk**: the game's own Quiet Park, recoloured to evening. A stage's lighting is in
  its vertex colours, so changing those recolours the whole stage without moving a triangle.

## Replacing one of the game's stages

Under **Add stages**, **In place of one of the game's stages**: pick a stage, press **Replace it with a
stage folder**, and pick a folder like the ones **Import a stage folder** takes.

The folder is copied to `UNI2-IM\Mods\bg\bgNNN` under the game's own number. The stage keeps that
number, its card, its walls and whether the picker offers it, so it needs no free number. Files the
folder does not have still come from the game.

Its `stage.txt` changes the stage's values like an imported stage's does, except `StageW`,
`BlanchStage`, `BlanchChara`, `SelectDisable`, `RandomDisable`, `VsDisable` and `DLCFlag`. Those stay
the game's, because they decide where the walls are and which stage is played.

A `stage.txt` on its own in `Mods\bg\bgNNN`, for one of the game's numbers, changes only that stage's
values. Nothing else has to be in the folder.

**Restore**, on the stage's row under **Installed stages**, deletes the folder and puts the game's
values back. Deleting the folder by hand does the same at the next start. Restart after either.

An opponent who does not have the replacement sees the game's own stage.

## MBAACC

Not offered. Its backgrounds are 2D sprite layers in a `bgmake` container that has nothing in common
with UNI2's format, and UNI2 has no 2D stage to put them in. Its **music** can be imported from the
[BGM selector](BGM-selector).

## Limits

- Stage numbers 28 to 89 are free, so you can have 62 ports. Only 28 to 47 get their own card.
- Added stages are only on your machine. Do not expect them to work online. A
  [replaced stage](#replacing-one-of-the-games-stages) keeps the game's number, so an opponent
  without it sees the game's own stage.
- Online, Random picks only from the game's own stages, so both players land on the same one.
- Hilda's UNI1 stage is not one of the two hidden stages. Stage 23 is French-Bread's own new Hilda
  stage.

## The model underneath

[Stage models](Stage-models) explains every file a stage is made of, the `fbxex` container that
holds the 3D model, and how to export that model to Wavefront OBJ, edit it in Blender and put it
back.
