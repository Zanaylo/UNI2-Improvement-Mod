# Stage models

This page covers the files a UNI2 stage is made of, the container its 3D model lives in, and how to
open that model in a normal 3D editor and save it back.

To install a stage from another game, see [Stages](Stages). This page explains the format underneath.

## What a stage is

A stage is a folder, `bg\bgNNN`, with loose files in it. There is no archive and nothing is encrypted.

| file | what it is |
|---|---|
| `bg.fbx.bin` | the 3D model, in French-Bread's own `fbxex` container |
| `*.dds` | the textures, DXT1, DXT3 or DXT5, no mipmaps |
| `object.txt` | the 2D object layer: which `.pat` to use and how its sprites move |
| `*.pat` | the sprite sheet the object layer draws, `PAniDataFile` |
| `stage_color.img` | 4096x2 of pure white, byte-identical in every stage that ships it. **The game never opens it.** The name is not in `uni2.exe` |
| `stage_specular.img` | a 4096x2 specular ramp, per stage. Read by the character shader, not the stage one |
| `stage_bokashi_alpha.img` | a 4096x1 blur ramp, per stage. Read by the character shader, not the stage one |
| `bg.fbx.json` | a readable dump of the model. A build leftover: **the game never opens it** |
| `bg.fbx` | the source Autodesk FBX. Only six stages ship one |

The stage's numbers (position, scale, field of view, fog, bloom) are not in the folder. Each stage
has one block for them in `bg\BgList.txt`.

An `.img` is a 20-byte header (two zeros, `7`, format `2`, then width and height) followed by RGBA8.
`bg027` ships none of the three and looks correct, so all three are optional.

## The `fbxex` container

`bg.fbx.bin` starts with `fbxex\0\0\0` and two zero dwords, then four blocks in a fixed order. Each
block is a dword size (it counts its own 8-byte prefix), a dword count, then the body. The last block
ends exactly at the end of the file.

```
'fbxex\0\0\0'  dword 0  dword 0
  block 0  texture
  block 1  material
  block 2  node
  block 3  anime
```

**texture:** `count` records of a 128-byte name. The index is the position.

**material:** `count` records of 204 bytes: a 128-byte name, a dword index, a dword texture index,
then 17 floats. The 17 are Diffuse(4), Ambient(4), Specular(4), Emissive(4), Power(1), the same
layout as `D3DMATERIAL9`. The engine does not seem to use the diffuse as a material colour. The
fourth slot of each colour is a "present" flag: 1.0 when the source FBX had that property, 0.0 when
it did not. That is why lambert materials have `value[11] = 0`. The material block is the flat
submesh list: one entry per submesh, in node order.

**node:** `count` records. Each is a dword size (it counts its own 16-byte prefix), a dword type, a
dword first child and a dword next sibling, then the payload. Node 0 is the scene root and has no
payload. Child and sibling are indices into this block, `-1` for none.

- type 0 is a plain transform, payload empty.
- type 1 is a mesh:

```
dword flag, dword flag, 4x4 float matrix, dword vertexCount,
vertexCount * 12 floats, dword submeshCount,
submeshCount * (dword material, dword indexCount, indexCount dwords)
```

The first flag is the blend mode: 1 draws the mesh additively. The second is 1 on only 26 of the
5077 meshes UNI2 ships, and nobody knows yet what it means. The matrix is the node's **world** matrix.
The vertices are in the mesh's own space and this matrix places them.

A vertex is 12 floats: position(3), normal(3), colour(3), alpha(1), UV(2). Across all 419827
vertices the game ships, the normals are always unit length, colour and alpha always stay in [0,1], and the UVs go
well past it, as tiling UVs do. Indices are a triangle list into the mesh's own vertices.

**anime:** `count` records, one per node including the root. Each is a dword frame count, then that
many 4x4 matrices. A count of 1 is a still node holding its local matrix. More than 1 is a track that
loops on its own length. **22 of the 28 shipped stages mix several track lengths in one stage**:
`bg005` runs 5, 10, 12, 20, 25, 120, 240 and 4800 side by side. Tracks hold **local** matrices,
sampled at 30 per second.

The mod reads and writes this container, and rebuilds all 28 shipped stages byte for byte.

## Blender

**`io_scene_fbxex.zip`** is a Blender add-on that reads **and writes** `bg.fbx.bin`. It is not in
the mod's download. You find it in the source repository, under `resource\blender`, next to a README
that covers the same ground as this page.

**Install the zip, not a loose `.py`.** A bare script does nothing or shows up as a legacy add-on you
have to tick by hand. Go to **Edit > Preferences > Add-ons**, open the arrow at the top right, choose
**Install from Disk** and pick the zip. It turns itself on. You need Blender 4.2 or newer.

- **File > Import > FbxExp model (.fbx.bin)** builds one object per mesh node, in its own
  collection. Each material gets the stage's own DDS, the per-vertex shade is wired in, and additive
  nodes are set to add instead of blend. The object's transform is the node's world matrix, so the
  stage stands the way the game draws it.
- **File > Export > FbxExp model (.fbx.bin)** writes it back.

**Import and then export with no edit gives the same file byte for byte, on all 28 shipped stages.**

### If you have never used Blender

The add-on does the hard part. You only need four bits of Blender, and they take five minutes.

- **Moving something.** Left-click an object to select it. Press `G` and move the mouse to drag it,
  then left-click to drop it. Press `X`, `Y` or `Z` after `G` to lock it to one axis, and type a
  number to move it exactly: `G` `X` `0.2` `Enter` slides it 0.2 along X. `R` rotates and `S`
  scales the same way. `Escape` cancels a move you have not dropped yet.
- **Seeing what you are doing.** Hold the middle mouse button to orbit, scroll to zoom, and hold
  shift with the middle button to pan. Numpad `1` looks at the stage from the front, close to where
  the game's camera is. Press `Z` and pick **Material Preview** to see the textures instead of grey.
- **Finding one thing among hundreds.** The Outliner, top right, lists every object by name. The
  importer names them `node000`, `node001` and so on, in the container's order, with `_additive`
  on the ones that glow. Click a name to select it in the viewport.
- **Undo is `Ctrl+Z`**, and it goes back a long way.

You cannot break the stage you imported. Export only writes where you tell it to, and the original
`bg.fbx.bin` stays untouched until you choose to overwrite it. Copy the stage folder before you
start and you have nothing to lose.

### Adding an object

A mesh with no `fbxex_node` property is **new**, and export adds it. It writes a mesh node under the
scene root, a material record per material slot, a texture entry for its image if the stage does not
already have that file, and a one-frame `anime` record so the block still has one entry per node.
The object then gets its own `fbxex_node`, so exporting twice does not add it twice.

Step by step:

1. **Import the stage you want to add to.** File > Import > FbxExp model, and pick
   `UNI2-IM\Mods\bg\bgNNN\bg.fbx.bin`. Use a port of your own, not the game's own `d` archive.
2. **Make the object.** Add > Mesh > whatever, or bring one in from another file. Put it where you
   want it. The game uses the object's transform.
3. **Give it a material with a texture.** In the Shading tab, add an Image Texture node and open a
   `.dds` **from that stage's own folder**. Only the file name is written, so the stage must already
   have that file, or you must put your own `.dds` next to the others. The easy way is to reuse a
   texture the stage already has.
4. **UV unwrap it** (U > Smart UV Project is fine). Without UVs the object has no texture.
5. **Export.** File > Export > FbxExp model, over the same `bg.fbx.bin`. The report line tells you
   how many nodes were written, moved and added.
6. **Restart the game** and pick the stage.

Two things to know. The exporter writes the material's **image file name**, so a texture that is not
in the stage folder will not load. The container cannot carry the image itself. Also, a new object
sits flat under the scene root and does not move. You cannot animate it here.

### What it cannot do

- **Deleting nodes.** If you remove an object from the scene, its node stays in the file as it was.
  To hide something, move it out of shot or scale it to nothing.
- **Editing the animation.** Tracks are kept, and shifted when you move the node, but you cannot
  author them.
- **Editing the material's numbers.** The 17 floats of a material record come from the template. A
  new material gets a plain white diffuse.

For the stage to load, save the result over `UNI2-IM\Mods\bg\bgNNN\bg.fbx.bin`. Importing the
stage again from the mod's panel overwrites it.

## BBTAG and CENTRALFICTION models

`io_scene_mua.zip`, in the same folder, opens the `.MUA` models of BLAZBLUE CROSS TAG BATTLE and
CENTRALFICTION with their motions, scripts and textures, and can save them back.

To play one of those stages in UNI2, use **Get stages from another fighting game** in the Stages
panel. Point it at the game folder and it converts the stages for you. It plays each stage's scripts
the way BBTAG does and rolls the same random picks as the Mua add-on, so lights fade, TVs change
channel and lightning flashes in the port like they do in Blender. A sprite only uses the frames drawn
on its own sheet, and it can't be half faded: it's shown or it isn't. To fix a port by hand:

1. Open the port, `UNI2-IM\Mods\bg\bgNNN\bg.fbx.bin`, with the FbxExp add-on.
2. Open the original `<stage>_vtx.pac` with the Mua add-on next to it, to see how it should look and
   move.
3. Fix the port and export back over the same `bg.fbx.bin`.

The Mua add-on also exports a normal FBX (**File > Export > Mua model as FBX**) for any other
program. The UV scroll and the sprite sequences don't fit in an FBX, so they're left out.

## Coordinates and conventions

- Row-vector matrices: a point is `v * M`, and the translation sits in elements 12, 13 and 14.
- The node matrix is world, the anime track matrices are local.
- Triangle lists, no strips. The source FBX's polygons are triangulated as a fan.
- UV origin is top-left in the container.
- Vertices are not shared between corners. The converter writes one per polygon corner, so vertex
  counts are about twice the triangle count.

## The source FBX side

Six shipped stages include the Autodesk FBX they were built from next to the converted result:
`bg020`, `bg021`, `bg022`, `bg023`, `bg027` and `bg090`. Converting the first and comparing it to the
second is the only real test of the conversion.

The mod's own converter reads FBX 6.1.0 ASCII. Every DFCI stage uses that flavour, and so do five of
those six. It follows these rules:

- **Node order** is a depth-first walk of the scene graph, with children in declaration order.
- **`blendmode` is the model's user-defined `adding` property**, not anything in the material or the
  texture. On `bg020` this is exact both ways: all 42 meshes with it are additive, all 157 without it
  are not.
- **Colour factors are ignored.** `DiffuseFactor` and friends do not multiply the colour.
- **An absent colour property leaves all four slots zero.** That is what makes the fourth slot a
  "present" flag.
- **UV `v` is copied, not flipped.**
- **Animation is 30 fps**, sampled on whole frames from each node's own first key. A track's length
  is that node's own key span, not the take's.
- **`InheritType` is ignored, on purpose.** Every model in these files asks for RSrs, but
  French-Bread's own converter used RrSs anyway. On `bg020`, the three meshes where the two differ
  match the shipped `.bin` to 2.4e-7 under RrSs and are off by 6.1e-3 under RSrs. Following the flag
  would make the mod's output disagree with the shipped files.
- `Key` records have variable length and cannot be read with a fixed stride. The interpolation and
  tangent letters are the only boundary markers. Keys are cubic, so the tangents matter.

`bg021` is the one stage in FBX 7700 form, with `Properties70`, UID-first object headers and
`KeyTime` arrays. The converter does not read that flavour. No DFCI stage needs it.

## Things the engine does that will surprise you

- **UNI2 grades every background.** It draws `0.10 + tex * In.color`, with `In.color` near 0.69.
  UNI2's own picker cards show it too: they never go below 26 to 30 luma. Textures made for a
  renderer that draws them 1:1 look lighter and less saturated here. The mod undoes this while the
  game runs, per stage, and its defaults match the game exactly.

- **Fog does not check `IsFog`.** `Shader\fbxshader.txt` runs its fog branch when
  `g_FogColor.a >= 0.001f`, and the blend is an unclamped `lerp`. Fog values left over from another
  stage push each channel to its own clamp and posterise the whole background. The only value that
  means "no fog" is an alpha of zero.
- **`bg.fbx.json` is never read.** The loader builds `%s\%s\bg.fbx` and adds `.bin`.
