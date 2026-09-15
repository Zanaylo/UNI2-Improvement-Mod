# Blender add-ons

Two add-ons live in this folder.

- **FbxExp model** (`io_scene_fbxex.zip`) opens and saves `bg.fbx.bin`, the stage model used by
  UNDER NIGHT IN-BIRTH II, MELTY BLOOD: TYPE LUMINA, UNI[st] and DFCI.
- **Mua model** (`io_scene_mua.zip`) opens the `MUA` models of BLAZBLUE CROSS TAG BATTLE and
  BLAZBLUE CENTRALFICTION, with their skeleton, animation and scripts, and saves them back.

**The Mua one is still a work in progress.** Adding or removing a mesh or a material, bones and
weights on a mesh whose vertex count changed, the collision box scale, sprite sheet swaps and taking
animation back out of Blender don't work yet.

## Install

You need Blender 4.2 or newer.

1. `Edit` > `Preferences` > `Add-ons`.
2. Open the arrow at the top right and pick `Install from Disk...`.
3. Choose the zip.

It turns itself on. On Blender 4.1 or older, use `Install...` and tick the add-on in the list.

Install the zip, not the `.py`. A loose script either does nothing or shows up as a legacy add-on you
have to turn on by hand.


# FbxExp model

Adds `File` > `Import` > `FbxExp model (.fbx.bin)` and `File` > `Export` > `FbxExp model (.fbx.bin)`.

Importing and exporting without any change gives back the same file, byte for byte, on every stage
the game ships.

## Open a stage

Import a `bg.fbx.bin`, or just pick the stage folder. The textures load from the `.dds` files next to
it, so import the model where it is instead of copying it somewhere else.

You get:

- one object per mesh node (nodes without a mesh aren't built and nothing is parented, the node tree
  stays in the file)
- the stage's vertex colours, which is where its lighting lives
- materials using the stage's textures
- the animation, if the stage has one

## Save it back

Export over the stage you imported. The exporter needs a **template**, the `.bin` you imported,
because Blender can't hold the material table, the node tree or the animation. Those come from the
template and only the geometry is replaced. The field is filled in for you, so only set it when
you're exporting into a different stage.

Objects you add become new nodes. Objects you move keep their new position. Objects you didn't touch
go back exactly as they were.

## Rules the engine has

- **Node 0 has to stay a root.** If a mesh ends up first, the stage shows up as an empty green field.
  Don't delete the top empty.
- **Winding matters.** The engine culls back faces, so a floor facing the wrong way disappears.
  `Mesh` > `Normals` > `Recalculate Outside` fixes it.
- **Textures have to be `.dds`** and sit in the stage folder.
- **There are no lamps.** The shader is roughly `lift + texture x vertex colour`, so Blender's
  lighting isn't what you'll see in game. Paint the vertex colours to light a stage.
- **Scale.** A character is about **0.13 units tall** in the model, around 1.3 on screen after the
  stage's own `Scale` (usually 10). Use that as your ruler: a corridor three characters tall is a tall
  corridor. At the back wall the screen shows roughly **x -0.46 to +0.46**, with the floor about two
  thirds of the way down. Anything outside that box only shows when the camera pans.

## Object settings

Under `Object Properties` > `Custom Properties`:

- `fbxex_blendmode`: `0` normal, `1` additive. Fire, glow and light shafts are additive.
- `fbxex_flag1`: a second flag the game sets on 26 of its several thousand meshes. Nobody knows what
  it does yet, so leave it.
- `fbxex_matrix`: where the object was when you imported it. The exporter uses it to tell whether you
  moved the object, so deleting it makes the object export as moved.
- `fbxex_node`: which template node the object came from. Delete it and the object exports as a new
  node.

The mesh has three more under `Object Data Properties`. `Shade` is the colour attribute holding the
stage lighting, and it's the one to paint. `fbxex_normal` and `fbxex_rest` keep the original normals
and positions, so a vertex you didn't move keeps the normal it shipped with.

The scene keeps `fbxex_source`, the file the export template defaults to.

## Material settings

Each material entry has **17 floats**: Diffuse (4), Ambient (4), Specular (4), Emissive (4) and
Power (1). You'll find them under `Material Properties` > `Custom Properties`:

- `fbxex_diffuse`, `fbxex_ambient`, `fbxex_specular` and `fbxex_emissive`, four floats each
- `fbxex_power`, the specular power

Edit them there and the exporter writes them into the template's material table. Values you don't
touch go back as they were.

There's **one Blender material per entry**, named `mNNN <texture>`, because a stage can have several
entries using the same texture with different values. The exporter matches them by **slot order**,
not by name, so renaming a material is safe.

A material without these properties, like one you made yourself, keeps the entry the template
already had.

Whether the game really uses these values is another story. `Shader/sh_bgspeculer.txt` has a
specular and a non specular technique and nobody knows yet which mesh gets which, so a Power change
might do nothing. Vertex colours (`Shade`) and Lift/Contrast in the mod's Stages tab are what's known
to work.


# Mua model

Adds `File` > `Import` > `Mua model (.mua, .pac)`, `File` > `Export` > `Mua model (.mua)` and
`File` > `Export` > `Mua model as FBX (.fbx)`.

## Open a model

Pick a stage's `<stage>_vtx.pac` from `<BBTAG>\data\bg\<group>\`, the scene `<stage>.pac` next to
it, or a bare `.MUA`. **CENTRALFICTION works too.** Its `data\BG\main_cf\` archives are the same thing
inside a `DFAS` zlib wrapper, and the add-on unwraps it.

Everything else is found from there: the geometry, the textures (pulled out of `_img.pac` into a
`<stage>_tex` folder, since Blender needs them on disk), the motions, the camera motion and the
scripts. A file that isn't `FPAC` or `MUA` is treated as an encrypted retail file and decrypted by its
file name, so a model named by its MD5 opens as it is.

**The Timeline looks empty, and that's fine.** Blender only shows keys for the selected object's
active action. The takes are NLA strips on the armature, which is hidden, and no action is active.
To see them, unhide `<stage>_bones` in the outliner, select it and open the **NLA editor**. There's
one track per script. `Tab` on a strip enters tweak mode and the Dope Sheet shows that take's keys.
Wall lamps have keys too, on `mua_ramp`. You don't need any of this to watch it, just press `Space`.

**Materials are opaque unless something is really see-through.** Stages keep 255 in vertex alpha
and use it to fade the odd sprite. A lobby avatar has 127, 63 and 0 in there and isn't see-through at
all. So vertex alpha only counts as opacity when a model uses the top of the range. This only matters
in the viewport, where a material with transparency gets EEVEE's dithering all over the model.
Materials are named `mNNN <texture>` when opaque, with ` alpha` added when they fade and ` add` when
they're additive.

**Light quads are drawn additive**, like the game does. Stage lights are a bright middle on black,
so blended normally they'd be black slabs with a glow in them. Those materials end in ` add`, and
`fbxex_blendmode` on the object says `1` for additive and `0` for blended.

**The material does what the game's shader does**: texture times vertex colour, with the alpha
coming from both. The mapping node is left at identity so it can carry the UV scroll. `Workbench`
ignores that node, so if a flat render and the viewport ever disagree, check the mapping node.

**The armature is hidden after import**, so you see the model instead of a pile of bones. The meshes
still deform. Unhide `<stage>_bones` to pose it.

**8x8 DDS textures are decoded by the add-on.** Blender can't open any of the fourteen the two games
ship, and Cycles paints them magenta (`bg_snowtown`'s icicles use one). Bigger ones are loaded by
Blender itself.

**Textures somewhere else.** A character or effect `MUA` only carries texture *names*. Point
**Textures from** at the folder or the archives that have them. The import tells you how many it
found and warns you when some are missing. When textures load, the viewport switches to **Material
Preview**, since `Solid` shows everything grey.

**It comes in with UNI2's units and facing, not BBTAG's.** A character is 213 BBTAG units against
UNI2's 0.132, Z is mirrored, the triangles are flipped so they still face forward, and V is flipped.
What you see is what a port will look like. The checkboxes turn each of those off, and
`Character height` sets the scale (lower it to make the model bigger).

The objects use the **same attribute names** as the FbxExp add-on (`Shade`, `fbxex_normal`,
`fbxex_rest`, `UVMap`), so the FbxExp exporter can write them out too.

## What you get

- **Meshes:** one object per `MUA` mesh, vertex colours in `Shade`, one material per material entry
  named `mNNN <texture>`.
- **Skeleton:** one armature with every bone, parented like the file has them, and every mesh skinned
  by its vertex weights. The rest pose is the model's own, so nothing moves until a take does.
- **Takes:** one action per `mot/*.mmot` whose bone the model has, one per `cammot/*.mmot`, and one
  for the model's own bone tracks in sections 8 and 9. Three of the four UNI stages keep all their
  animation there.
- **UV scroll:** section 7, as keyframes on the material's mapping node with a cycle modifier. The
  fountain water gets eight of them, Central Station's escalator gets one.
- **Scripts:** every `scr/*.evb` as a Text datablock. The commands the add-on understands are
  applied, see below.
- **Frame rate:** the scene is set to 60 fps from frame 0, the same clock BBTAG counts in.

## What the scripts do

- **`0x06` picks a motion.** Each script that uses it gets an **NLA track** with the strips where the
  script puts them, so Central Station's two trains take turns like in BBTAG instead of running at
  the same time. A pick naming a motion the stage doesn't ship (the parked trains) leaves a gap, and
  that gap is the parked pose.
- **`0x12` is a ramp.** It becomes a keyed `mua_ramp` on the objects the script drives, 0 to 1,
  looping with the script. The sixteen wall lamps and the door lights at Central Station get one. It
  isn't wired to a shader, it's just a number you can read.
- **`0x12` also marks a light.** A mesh whose script ramps its brightness is drawn additive, which is
  what makes the beach's sea foam look like foam instead of a black slab. Same for a surface that
  fades to black in its vertex colours with no alpha, with a quarter of its vertices pure black and a
  bright peak. Tower Center's road is 3% black and stays solid.
- **`0x0b` is a sprite rectangle.** The quad gets one copy per rectangle, each with its UV window set
  and its visibility keyed so only the current one shows. That's Central Station's departure board,
  on the 6000 frame loop the script sets. The second value is the **sheet**: `0` is the material's own
  texture, a name table indexes that, and anything else is the numbered texture next to it. That's
  how the beach bunny cycles through `usagi_00.dds` to `usagi_07.dds`. A script without `0x0f` loops
  on the sum of its own holds.
- **`bgobj` containers are listed too.** `scr/scr.bin` is a script with named commands (`EventHead`,
  `create`, `set_model`, `set_evt`, `setposx`, `setvel`, `req_se`, `particle`, `wait`) that places
  objects in the stage. `col/*.jonbin` is collision: the bitmap it was drawn over, the canvas and a
  list of boxes. The boxes **aren't** in model units (2.56 and 2.75 times the model on the two torii,
  1.23 across and 3.70 up on the snow floor), so they're listed and nothing gets placed from them.
- **`base.evb` and `setting.evb`** are only listed. Their values aren't understood yet, and guessing
  would put wrong numbers in the file.

Scripts are tied to meshes by the **model's own skeleton record**, not by name. That's why
`boardtext_1.evb` hits exactly the two scrolling quads and nothing else. Each object shows the one it
got in `mua_script` under `Object Properties` > `Custom Properties`, next to `mua_mesh`,
`mua_skeleton` and `mua_sheet`. The material has `mua_material` (the record's eighteen floats) and
`mua_texture`.

## Save a model back

`File` > `Export` > `Mua model (.mua)`. It needs a **template**, the model you opened, and the field
is filled in for you. Everything Blender can't hold comes from it: bones, weights, materials,
animation, scripts, tangents and every section the add-on doesn't read. Only the geometry is
replaced.

**A model you didn't change comes back byte for byte.** All 34 BBTAG stages and all 91
CENTRALFICTION ones write back identical, and Central Station does it through Blender too. Vertices
you didn't move keep their exact bytes, and parts whose triangles you didn't touch keep their
original strips.

**Every model in the archive comes in and goes back out.** A `bgobj` archive keeps its models in
`mdldat/mdl/`, a lobby avatar in `mdl/` and a stage at the top level. Each one gets its own
collection and armature, and its materials are prefixed with its name so two models never share an
`m000`. `bgobj/test/bg_test_1.pac` has sixteen of them.

**Save it as a `.pac` to write the archive the game reads.** It's rebuilt at whatever depth the model
sits, with everything else in it (other models, scripts, motions, collision) copied over untouched.
The dialog opens on the model's own archive, so saving over it puts the model straight into the game
folder. The last sixteen bytes of an archive are a signature the add-on can't recompute, so they're
kept as they are. Every archive in both games (631 of them) rebuilds byte for byte when nothing
changed. A CENTRALFICTION one gets wrapped again, so it comes out a bit smaller with the same
contents.

Moving an object in object mode counts as an edit, same as moving its vertices.

**The scene archive gets written too.** A stage keeps a second copy of its model in `<stage>.pac`,
with the same mesh records minus the geometry. Those records hold each mesh's bounding box, so moving
something changes both copies. The export writes that copy as well and tells you so.
`Update the scene archive` turns it off.

**Keep the vertex and triangle counts the same** for now. Moving things is fine, but adding or
deleting vertices breaks the counts stored in those records.

It can't add or remove a mesh, add a material, or rewrite bones and weights on a mesh whose vertex
count changed.

### Seeing it in the game

These paths assume a BBTAG install with its `data` folder decrypted.

1. **Back up the archive.** Copy `...\BBTAG\data\bg\main_uni\bg_odaiba_vtx.pac` to
   `bg_odaiba_vtx.pac.bak`, because the export writes over the game's file. If you repacked that
   archive by hand before, start from a clean copy, since other repackers lay it out differently.
2. **Install the add-on.** It should show up as **Mua model (.mua) 0.5.0**.
3. **Import** `bg_odaiba_vtx.pac`.
4. **Export it straight back** without changing anything. The dialog opens on the archive you came
   from, so keep the name and save over it. The result is identical to the original, so if something
   breaks later it's your edit and not the add-on. Start the game once and check the stage still
   loads.
5. **Make one change you can't miss.** `vender_00` is the drinks machine on the platform. Select it
   in the outliner, press `G`, `Y`, type `0.15` and `Enter`. **Up is Y here**, because a `MUA` is Y
   up and comes in unrotated. The machine now floats a character's height above the floor.
6. **Export over the same `.pac` again**, start the game and pick Central Station.

## Save an FBX

`File` > `Export` > `Mua model as FBX`. It's Blender's own FBX exporter with the axis sorted out for
you. A `MUA` is Y up, so it lies on its back in Blender, and Blender's exporter writes the axis from
its settings instead of from the data. The scene is stood up just for the export, so the file opens
upright anywhere.

`Animation` writes one FBX take per **NLA strip** by default, which is how the scripts arranged them.
`Textures` writes **PNG files into a `<name>.fbm` folder next to the FBX** by default, because stage
textures are `DDS` and most programs can't open that. `Embedded as PNG` puts them inside the file,
and `The DDS as they are` just points at the originals.

The UV scroll and the sprite sequences don't make it into the FBX, because FBX has nowhere to put
them. Takes that barely move are dropped too: two of Riverside Space's four takes shift a light by
4e-6, which is just float noise.

## Fixing a ported stage by hand

The FbxExp exporter needs a `bg.fbx.bin` as its template, so this is how you fix a stage the mod
ported:

1. Import the port, `UNI2-IM\Mods\bg\bgNNN\bg.fbx.bin`, with **FbxExp model**.
2. Fix what's wrong. A light quad's blend flag is `fbxex_blendmode` on the object, a material is
   `mNNN <texture>` and the lighting is the `Shade` colour attribute.
3. Export back over the same `bg.fbx.bin`.

Open the BBTAG original next to it with the Mua add-on when you want to see how the port should look,
including the animation a static port loses.

## Rebuilding the zips

`package.py` builds both zips from the `.py` files in this folder:

    python package.py
