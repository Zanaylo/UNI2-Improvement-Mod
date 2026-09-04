# Stages

**Stages** on the main window, then **Open stages**.

Two things live here: the stages UNI2 hides from its own picker, and stages taken out of another
French-Bread game you own.

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

## MBAACC

Not offered. Its backgrounds are 2D sprite layers in a `bgmake` container that shares nothing with
UNI2's format, and UNI2 has no 2D stage to put them in. Its **music** does import, from the
[BGM selector](BGM-selector).

## Limits

- Stage numbers 28 to 89 are free, so that is 62 ports. Only 28 to 47 get their own card.
- Ports are yours alone. Do not expect them to work online.
- Hilda's UNI1 stage is not one of the hidden two. Stage 23 is French-Bread's own new Hilda stage.
