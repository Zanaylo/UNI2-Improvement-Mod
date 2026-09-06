# FbxExp stage — Blender add-on

Opens the stage models of UNDER NIGHT IN-BIRTH II, MELTY BLOOD: TYPE LUMINA, UNI[st] and
DFCI in Blender, and writes them back. The file it reads and writes is `bg.fbx.bin`, the
container French-Bread's own exporter produces; the `.fbx` next to it in a few stages is
only the source it was built from.

It both imports **and** exports. Every stage the game ships was imported and exported
again byte for byte, so a round trip that changes nothing changes nothing on disk.

## Install

1. Open Blender (4.2 or newer).
2. `Edit` > `Preferences` > `Add-ons`.
3. The arrow at the top right > `Install from Disk...`.
4. Pick `io_scene_fbxex.zip` from this folder.

It enables itself. Two entries appear:

- `File` > `Import` > `FbxExp stage (.fbx.bin)`
- `File` > `Export` > `FbxExp stage (.fbx.bin)`

If `Install from Disk...` is missing you are on Blender 4.1 or older; use `Install...` and
tick the add-on in the list afterwards.

## Open a stage

Import any `bg.fbx.bin`. The textures load from the `.dds` files beside it, so import the
model where it sits rather than copying it out on its own.

What arrives:

- one Blender object per node, parented the way the stage parents them
- the stage's own vertex colours, which is where a stage keeps its lighting
- materials pointing at the stage's textures
- the animation, if the stage has one

## Write it back

Export over the stage you imported. The exporter needs a **template** — the `.bin` the
scene came from — because a Blender scene cannot hold everything the container does: the
material table, the node tree and the animation are kept from it and only the geometry is
replaced. The field is filled in for you with the file you imported; set it by hand only
when exporting into a different stage.

Objects you add in Blender become new nodes. Objects you move keep their new position.
Objects you did not touch are written back unchanged, down to the last bit.

## Rules the engine imposes

- **Node 0 must stay a root.** If a mesh ends up first the stage renders as an empty green
  field. Do not delete the topmost empty.
- **Triangle winding matters.** The engine culls back faces, so a floor wound the wrong way
  disappears. In Blender, `Mesh` > `Normals` > `Recalculate Outside` on a selection fixes it.
- **Textures must be `.dds`** and must sit in the stage folder. Nothing else is read.
- **A stage has no lamp.** The shader is roughly `lift + texture x vertex colour`, so
  Blender's lighting is not what you will see. Paint the vertex colours to light a stage.
- **Scale.** A character is about 1.6 units tall. Stages are built around that.

## Per-object settings

Selecting an object shows these in `Object Properties` > `Custom Properties`:

- `fbxex_blendmode` — `0` normal, `1` additive. Additive is how fire, glow and light shafts
  are drawn.
- `fbxex_flag1` — a second flag the game sets on 26 of the several thousand meshes it
  ships. Other people's tools call it `alpha`; nobody has measured what it does. Leave it.
- `fbxex_matrix` — where the object sat when it was imported. It is how the exporter knows
  an object was never really moved, so leave it alone; deleting it makes the object export
  as moved.

## Rebuilding the zip

`package.py` writes `io_scene_fbxex.zip` from `io_scene_fbxex.py`, so the installed add-on
is never a second copy that drifts from the source:

    python package.py
