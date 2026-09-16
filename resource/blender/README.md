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

**The model says how every surface is drawn, and nothing is guessed.** Each mesh's skeleton record
carries the blend mode BBTAG's own draw puts on it (`UNI2-docs/BBTAG-DECOMPILE.md`):

- `0` is **opaque**. The game still throws away texels whose alpha is exactly 0, so a cut-out
  texture keeps its holes. The material is named `mNNN <texture>` and drawn Dithered, which for a
  0-or-1 alpha is a clean cut-out.
- `2` is **additive**, named ` add`, scaled by its alpha the way BBTAG adds, so light rays fade at
  their edges. `fbxex_blendmode` on the object says `1`.
- `4` is **subtractive**, named ` sub`. Blender can't subtract, so it darkens what's behind it.
- **Anything else**, the "not set" value on most meshes and every mesh of Tower Center included, is
  **alpha blended**. That is the game's default for a model, not a judgement about the texture.
  BBTAG still writes depth for it, and Blender has no blending that writes depth, so the add-on
  picks the closer of Blender's two ways:
  - ` alpha`, drawn Dithered, when the surface is fully there or fully gone. Depth is right, so
    nothing shows through the wall in front of it.
  - ` blend`, drawn Blended, when it's really see-through: a texture that is mostly soft alpha,
    vertex alpha that fades, or a script that holds it half faded. Every surface of it draws, so
    a glow card or a shadow behind another one still shows through.
  - ` sheer`, drawn Blended, when the skeleton's flags turn depth writing off (`0x2000`).
  - ` add`, when nothing about the surface is see-through but it draws black at every vertex while
    its texture has light in it: Central Station's lens flares, Station 2's search light, Duel
    Field's run lights and the boss stage's fireballs. BBTAG's draw doesn't mark them, but the game
    shows them without a black card, so they're added like a light.

**A glow card whose polygon stops before its texture fades out** keeps a faint alpha along its
border, and Blender draws that as a hard edge the game doesn't show (Tower Center's street lights,
Riverside's lamps). The add-on measures the alpha left along the mesh's border and puts it in
`mua_rim`, and the ` blend` and ` sheer` materials take it off, so the border lands on exactly 0.
It only does that when the texture's own outer ring is clear and what's left is a faint tail, at
most about a fifth of the texture's strongest alpha. Set `mua_rim` to `0` to see the card as it is.

The object keeps the number in `mua_blend`, the skeleton's flag word in `mua_flags` and the border
trim in `mua_rim`. A mesh whose
flags hide it (`0x20`) isn't built.

**A material can have up to three textures.** The first slot is the base. The third is a reflection,
added on top as a sphere map: that's what lights Ishana's crystals and Snowtown's icicles. A texture
in an unused slot, like Lakeside's missing `zhong.dds`, is skipped.

**The material does what the game's shader does**: texture times vertex colour, with the alpha
coming from both. The vertex colour is decoded before it multiplies, since the game multiplies the
raw bytes, and the scene is set to the `Standard` view, since Blender's default `AgX` dulls every
colour. The mapping node carries the UV animation exactly as the game's vertex shader applies it:
u moves against the animated offset, v with it, and the scale divides. `Workbench` ignores that
node, so if a flat render and the viewport ever disagree, check the mapping node.

**The armature is hidden after import**, so you see the model instead of a pile of bones. The meshes
still deform. Unhide `<stage>_bones` to pose it.

**8x8 DDS textures are decoded by the add-on.** Blender can't open any of the fourteen the two games
ship, and Cycles paints them magenta (`bg_snowtown`'s icicles use one). Bigger ones are loaded by
Blender itself. A texture whose header wrongly marks it as a volume, like Station's `mirror_sky.dds`,
is loaded from a copy with that mark cleared and packed into the file.

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

**Each script runs the way BBTAG runs it**, one copy per skeleton, frame by frame
(`BB_CEventInstance`, `UNI2-docs/BBTAG-DECOMPILE.md`). `0x03` waits for a frame, `0x15` pauses the
script's clock while its sprites and ramp keep going, `0x0f` jumps back, `0x13` rolls a branch and
`0x05` starts a label whose `0x0b` holds play in order. What comes out is keyed and loops on the
script's own period, so Central Station's trains take turns every 6001 frames, the way the game
counts them.

- **`0x13` rolls a branch.** The roll uses a fixed seed per skeleton, so an import always comes out
  the same but every skeleton rolls its own: Town's lightning bolts strike at different times and
  every TV in Midnight Ring picks its own channel. A script that keeps rolling is keyed for a minute
  and repeats. When a roll would hide the object for good, like Garden's rabbits, which BBTAG skips
  half the time, it rolls again so you get to see it. The stage importer in the mod rolls the same
  way, so a port matches Blender.
- **`0x06` picks a motion.** Each script that uses it gets an **NLA track** with the strips where the
  script puts them. A pick naming a motion the stage doesn't ship (the parked trains) leaves a gap,
  and that gap is the parked pose.
- **`0x12` is a ramp on the alpha.** It's keyed on `mua_ramp`, which the material multiplies in, so
  lamps, sea foam and the P4 crowd fade in and out. A ramp that never loops plays once: Lakeside
  starts in daylight and its day copies fade out around frame 2500. While a ramp sits at 0 the
  object is hidden too, so it can't black out the copy underneath it.
- **`0x0b` is a sprite rectangle.** The mesh gets one copy per rectangle, each with its UV window set
  and its visibility keyed so only the current one shows, and every copy carries the ramp. That's
  Central Station's departure board and vending machine buttons, and every TV in Midnight Ring. The
  second value is the **sheet**, and the script names its sheets itself: the first name block of the
  `.evb` lists them in order (`usagi.evb` lists `usagi_00.dds` to `_07`, `saru_B.evb`
  `mob_saruB_0` and `_1`, `tv1.evb` `TV_suna`, `TV_base`, `TV_A_00`...). That is the texture the
  game binds, so nothing is looked up by name.
- **The rectangle is applied to the mesh's own UVs**, not stretched over them. The beach bunny's
  reflection samples the strip drawn under the bunny on the same sheet, so it comes out upside down
  like in the game.
- **A sheet a rectangle doesn't fit in is the live screen.** Duel Field's monitors cut 640 by 360
  pieces out of a 1280 by 720 frame, and BBTAG fills them with what's happening in the match.
  Blender can't do that, so those monitors keep the picture the stage ships.
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
2. **Install the add-on.** It should show up as **Mua model (.mua) 0.9.0**.
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
