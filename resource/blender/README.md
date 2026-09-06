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

## The Last Corridor, already set up

`last_corridor.blend` in this folder is a finished stage, opened and ready to edit. It is the
quickest way in: open it, move something, `File > Export`, and the stage is written. The scene also
carries two things you cannot see otherwise - a camera at the framing the game really uses, so the
viewport shows what a match will show, and an arrow the height of a character.

Its export Template is already pointed at the folder it was built from. **On another machine that
path will not exist**, so set the Template field in the export dialog to your own copy of the
stage's `bg.fbx.bin`.

`tools/blender/make_template.py` builds it, so any stage can be turned into one the same way.

## Open a stage

Import any `bg.fbx.bin`. The textures load from the `.dds` files beside it, so import the
model where it sits rather than copying it out on its own.

What arrives:

- one Blender object per mesh node. Nodes that carry no mesh are not built and nothing is
  parented in Blender - the node tree stays in the file you exported from, which is why the export
  needs it as a template
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
- **Scale.** Measured off a real frame: a character is about **0.13 units tall in the model**, which
  is roughly 1.3 units on screen once the stage's own `Scale` (usually 10) has multiplied it. Author
  in model units and use the character height as the ruler - a corridor three character-heights tall
  is a tall corridor. The screen shows about **x -0.46 to +0.46** at the back wall, with the floor
  line about two thirds of the way down, so anything outside that box is only seen when the camera
  pans.

## Per-object settings

Selecting an object shows these in `Object Properties` > `Custom Properties`:

- `fbxex_blendmode` — `0` normal, `1` additive. Additive is how fire, glow and light shafts
  are drawn.
- `fbxex_flag1` — a second flag the game sets on 26 of the several thousand meshes it
  ships. Other people's tools call it `alpha`; nobody has measured what it does. Leave it.
- `fbxex_matrix` — where the object sat when it was imported. It is how the exporter knows
  an object was never really moved, so leave it alone; deleting it makes the object export
  as moved.
- `fbxex_node` — which node in the template this object came from. Delete it and the object is
  exported as a brand new node instead of replacing the one it came from.

The mesh carries three more, under `Object Data Properties`: the colour attribute `Shade`, which is
the stage's lighting and is the thing to paint; and `fbxex_normal` and `fbxex_rest`, which hold the
original normals and positions so a vertex you did not move keeps the normal it shipped with.

The scene carries `fbxex_source`, the file the export Template defaults to.

## Per-material settings

A stage's materials are the one thing that used to need a hex editor. Each entry in the file
carries **17 floats** - Diffuse(4), Ambient(4), Specular(4), Emissive(4) and Power(1) - and the
importer now hangs them on the Blender material as custom properties, under
`Material Properties` > `Custom Properties`:

- `fbxex_diffuse`, `fbxex_ambient`, `fbxex_specular`, `fbxex_emissive` - four floats each
- `fbxex_power` - one float, the specular power

Edit them there and the exporter writes them back over the template's material table. Untouched,
they go back byte for byte, so a stage you only moved a prop in still exports identical.

**One Blender material per material entry.** They are named `mNNN <texture>` because a stage can
hold several entries pointing at the same texture with different values, and collapsing them by
name would have made those uneditable. The exporter matches by **slot order**, not by name, so
renaming a material is safe and an older `.blend` still exports correctly.

**A material with no such properties is left alone** - the entry the template already had is kept.
That is what happens to a material you make yourself in Blender.

**Whether the game draws with them is a separate question.** `Shader/sh_bgspeculer.txt` has both a
specular and a no-specular technique and which mesh gets which has never been measured, so a Power
change may show nothing. Vertex colour (`Shade`) and the per-stage Lift/Contrast in the mod's Stages
tab are the levers that are known to work.

## Rebuilding the zip

`package.py` writes `io_scene_fbxex.zip` from `io_scene_fbxex.py`, so the installed add-on
is never a second copy that drifts from the source:

    python package.py
