# Stage models

Everything a UNI2 stage is made of, the container its 3D model lives in, and how to take that model
out to a normal 3D editor and put it back.

For installing a stage from another game, see [Stages](Stages). This page is the format underneath.

## What a stage is

A stage is a folder, `bg\bgNNN`, holding loose files. There is no archive and nothing is encrypted.

| file | what it is |
|---|---|
| `bg.fbx.bin` | the 3D model, in French-Bread's own `fbxex` container |
| `*.dds` | the textures, DXT1, DXT3 or DXT5, no mipmaps |
| `object.txt` | the 2D object layer: which `.pat` to use and how its sprites move |
| `*.pat` | the sprite sheet the object layer draws, `PAniDataFile` |
| `stage_color.img` | 4096x2 of pure white, byte-identical in every stage that ships it. **The game never opens it** — the name is not in `uni2.exe` |
| `stage_specular.img` | a 4096x2 specular ramp, per stage. Read by the character shader, not the stage one |
| `stage_bokashi_alpha.img` | a 4096x1 blur ramp, per stage. Read by the character shader, not the stage one |
| `bg.fbx.json` | a readable dump of the model. A build leftover — **the game never opens it** |
| `bg.fbx` | the source Autodesk FBX. Only six stages ship one |

The stage's numbers — position, scale, field of view, fog, bloom — are not in the folder. They live
in one block per stage in `bg\BgList.txt`.

An `.img` is a 20-byte header (two zeros, `7`, format `2`, then width and height) followed by RGBA8.
`bg027` ships none of the three and renders correctly, so they are all optional.

## The `fbxex` container

`bg.fbx.bin` is `fbxex\0\0\0`, two zero dwords, then four blocks in a fixed order. Each block is a
dword size counting its own 8-byte prefix, a dword count, then the body. The chain ends exactly at
end of file.

```
'fbxex\0\0\0'  dword 0  dword 0
  block 0  texture
  block 1  material
  block 2  node
  block 3  anime
```

**texture** — `count` records of a 128-byte name, index is the position.

**material** — `count` records of 204 bytes: a 128-byte name, a dword index, a dword texture index,
then 17 floats. The 17 are Diffuse(4), Ambient(4), Specular(4), Emissive(4), Power(1) — the layout
of `D3DMATERIAL9`, though the engine does not appear to use the diffuse as a material colour. The
fourth slot of each colour is a present flag: 1.0 when the source FBX carried that property, 0.0
when it did not, which is why lambert materials have `value[11] = 0`. The material block is the
flattened submesh list: one entry per submesh, in node order.

**node** — `count` records, each a dword size counting its own 16-byte prefix, a dword type, a dword
first child and a dword next sibling, then the payload. Node 0 is the scene root and carries no
payload. Child and sibling are indices into this block, `-1` for none.

- type 0 is a plain transform, payload empty.
- type 1 is a mesh:

```
dword flag, dword flag, 4x4 float matrix, dword vertexCount,
vertexCount * 12 floats, dword submeshCount,
submeshCount * (dword material, dword indexCount, indexCount dwords)
```

The first flag is the blend mode: 1 draws the mesh additively. The second is 1 on only 26 of the
5077 meshes UNI2 ships and its meaning is not known. The matrix is the node's **world** matrix, so a
mesh's vertices are in its own space and this places it.

A vertex's 12 floats are position(3), normal(3), colour(3), alpha(1), UV(2). Measured across 419827
of them: the normals are unit length in 100% of cases, the colour and alpha never leave [0,1], and
the UVs run well past it the way tiling UVs do. Indices are a triangle list into this mesh's own
vertices.

**anime** — `count` records, one per node including the root, each a dword frame count then that
many 4x4 matrices. A count of 1 is a still node holding its local matrix. Anything more is a track
that loops on its own period: **22 of the 28 shipped stages mix several track lengths in one stage**,
`bg005` running 5, 10, 12, 20, 25, 120, 240 and 4800 side by side. Tracks hold **local** matrices,
sampled at 30 per second.

The mod reads and writes this container, and reproduces all 28 shipped stages byte for byte, which
is what makes it a writer and not just a parser.

## Blender

**`io_scene_fbxex.zip`** is a Blender add-on that reads **and writes** `bg.fbx.bin`. It is not in
the mod's download - it lives in the source repository, under `resource\blender`, next to a README
covering the same ground as this page.

**Install the zip, not a loose `.py`.** Blender 4.2 replaced add-ons with extensions, and **Install
from Disk** wants a zip with a manifest in it - handed a bare script it either does nothing or
leaves a legacy add-on you still have to tick by hand, which is why nothing appears under File >
Import. So: **Edit > Preferences > Get Extensions**, the dropdown at the top right, **Install from
Disk**, pick the zip. It enables itself. Blender 4.2 or newer.

- **File > Import > FbxExp stage (.fbx.bin)** builds one object per mesh node, in its own
  collection, with the stage's own DDS on each material, the per-vertex shade wired in, and the
  additive nodes set up to add rather than blend. A node's world matrix is the object's transform,
  so the stage stands up the way the game draws it.
- **File > Export > FbxExp stage (.fbx.bin)** writes it back.

**Import then export with no edit reproduces the file byte for byte, on all 28 shipped stages.**

### If you have never used Blender

The add-on does the hard part; what is left is four bits of Blender you can learn in five minutes.

- **Moving something.** Left-click an object to select it. Press `G` and move the mouse to drag it,
  then left-click to drop it. `G` then `X`, `Y` or `Z` constrains it to one axis, and typing a
  number after that moves it exactly - `G` `X` `0.2` `Enter` slides it 0.2 along X. `R` rotates and
  `S` scales the same way. `Escape` cancels a move you have not dropped yet.
- **Seeing what you are doing.** Hold the middle mouse button to orbit, scroll to zoom, shift and
  middle to pan. Numpad `1` looks at the stage from the front, which is roughly where the game's
  camera is. Press `Z` and pick **Material Preview** to see the textures instead of grey.
- **Finding one thing among hundreds.** The Outliner, top right, lists every object by name. The
  importer names them `node000`, `node001` and so on, in the container's own order, with `_additive`
  on the ones that glow. Click a name to select it in the viewport.
- **Undo is `Ctrl+Z`**, and it goes back a long way.

You cannot break the stage you imported from - export writes a new file only where you tell it to,
and the original `bg.fbx.bin` is untouched until you overwrite it deliberately. Keep a copy of the
stage folder before you start and there is nothing to lose.

### Adding an object

A mesh in the scene that has no `fbxex_node` property is **new**, and export appends it: a mesh node
hung off the scene root, a material record per material slot, a texture entry for the image you gave
it if the stage does not already carry that file, and a one-frame `anime` record so the block still
has one entry per node. It is then given an `fbxex_node` of its own, so exporting twice does not add
it twice.

Step by step:

1. **Import the stage you want to add to.** File > Import > FbxExp stage, and pick
   `UNI2-IM\Mods\bg\bgNNN\bg.fbx.bin` - a port of your own, not the game's own `d` archive.
2. **Make the object.** Add > Mesh > whatever, or bring one in from another file. Put it where you
   want it; the object's transform is what the game uses.
3. **Give it a material with a texture.** In the Shading tab, add an Image Texture node and open a
   `.dds` **from that stage's own folder**. The file name is what gets written, so the stage must
   already carry it or you must drop your own `.dds` in beside the others. Easiest is to reuse a
   texture the stage already has.
4. **UV unwrap it** (U > Smart UV Project is fine). Without UVs the object comes out untextured.
5. **Export.** File > Export > FbxExp stage, over the same `bg.fbx.bin`. The report line says how
   many nodes were written, moved and added.
6. **Restart the game** and pick the stage.

Two things worth knowing. The exporter writes the material's **image file name**, so a texture that
is not in the stage folder will not load - the container has no way to carry the image itself. And
a new object is added flat under the scene root and is static; it cannot be given animation here.

### What it cannot do

- **Deleting nodes.** Removing an object from the scene leaves its node in the file as it was. To
  hide something, move it out of shot or scale it to nothing.
- **Editing the animation.** Tracks are carried, and shifted when you move the node, but not
  authored.
- **Editing the material's numbers.** The 17 floats of a material record come from the template; a
  new material gets a plain white diffuse.

For the stage to load, write the result over `UNI2-IM\Mods\bg\bgNNN\bg.fbx.bin`. Re-importing the
stage from the mod's panel overwrites it again.

## Coordinates and conventions

- Row-vector matrices: a point is `v * M`, and the translation sits in elements 12, 13 and 14.
- The node matrix is world, the anime track matrices are local.
- Triangle lists, no strips. The source FBX's polygons are triangulated as a fan.
- UV origin is top-left in the container.
- Vertices are not shared between corners: the converter writes one per polygon corner, which is why
  vertex counts run about twice the triangle count.

## The source FBX side

Six shipped stages carry the Autodesk FBX they were built from as well as the converted result:
`bg020`, `bg021`, `bg022`, `bg023`, `bg027` and `bg090`. Converting the first and comparing against
the second is the only ground truth for the conversion.

The mod's own converter, `src/Game/FbxToFbxEx.cpp`, reads FBX 6.1.0 ASCII — the flavour every DFCI
stage uses and five of those six. Rules that are measured rather than assumed:

- **Node order** is a depth-first walk of the scene graph with children in declaration order.
- **`blendmode` is the model's user-defined `adding` property**, not anything in the material or the
  texture. On `bg020` this is exact in both directions: all 42 meshes carrying it are additive, all
  157 without it are not.
- **Colour factors are ignored.** `DiffuseFactor` and friends do not multiply the colour.
- **An absent colour property leaves all four slots zero**, which is what makes the fourth slot a
  present flag.
- **UV `v` is copied, not flipped.**
- **Animation is 30 fps**, sampled on whole frames from each node's own first key, and a track's
  length is that node's own key span — not the take's.
- **`InheritType` is ignored, on purpose.** Every model in every one of these files asks for RSrs,
  and French-Bread's own converter composed RrSs anyway: on `bg020` the three meshes where the two
  differ match the shipped `.bin` to 2.4e-7 under RrSs and are off by 6.1e-3 under RSrs. Honouring
  the flag would make the mod's output disagree with the only ground truth there is.
- `Key` records are variable length and cannot be read with a fixed stride; the interpolation and
  tangent letters are the only boundary markers. Keys are cubic, so the tangents matter.

`bg021` is the one stage in FBX 7700 form, with `Properties70`, UID-first object headers and
`KeyTime` arrays. The converter does not read that flavour. No DFCI stage needs it.

## Things the engine does that will surprise you

- **UNI2 grades every background.** It draws `0.10 + tex * In.color`, with `In.color` near 0.69 —
  measured by fitting a ported frame against the same stage in its own game, and confirmed by UNI2's
  own picker cards, which are in-engine renders and floor at 26 to 30 luma with nothing below.
  Textures authored for a renderer that draws them 1:1 come out lighter and less saturated here. The
  mod cancels it at run time rather than in the data: `uni2.exe` imports `d3dx9_42.dll`, so
  `Game/BgGrade` hooks `D3DXCreateEffect`, turns that `0.10f` into a `float4` pinned to `c220` and
  the texture product into one scaled by `c221`, and drives both through the device's
  `SetPixelShaderConstantF`. The defaults reproduce the game exactly, and the values are per stage.

- **Fog is not gated on `IsFog`.** `Shader\fbxshader.txt` runs its fog branch on
  `g_FogColor.a >= 0.001f`, and the blend is an unclamped `lerp`, so fog constants left over from
  another stage will drive each channel to its own clamp and posterise the whole background. The one
  value that means "no fog" is an alpha of zero.
- **`bg.fbx.json` is never read.** The loader composes `%s\%s\bg.fbx` and appends `.bin`.

