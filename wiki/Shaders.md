# Shaders

This tab sits next to Improvements. Everything on it only changes the picture. None of it touches
the simulation, the inputs, or anything your opponent can see.

**Every stage is off by default and each one has an explicit Off.** While all of them are off, the
mod does not read the back buffer at all and the frame is the game's own.

- **Upscale filter**: Off, bicubic, Lanczos or FidelityFX EASU, instead of the engine's bilinear
  stretch to your window. EASU follows the edges it finds, so a diagonal looks like a line instead of
  a staircase. It needs a back buffer larger than 1280x720, so raise the present size with it.
- **Anti-aliasing**: FXAA in five steps. Multisampling is not offered because it cannot work in this
  game. A Direct3D 9 texture cannot be multisampled and the whole scene is drawn into textures.
- **Bloom**: cuts out the bright parts of the frame, blurs them at a quarter size and screens them
  back on. This is the one setting here that looks like lighting instead of filtering. Use it for the
  neon in the stages and the glow on EXS and super effects.
- **Sharpening**: contrast adaptive, or FidelityFX RCAS to pair with the EASU upscale.
- **Colour and display**: brightness, contrast, gamma, saturation, vibrance, warmth, vignette,
  scanlines and dither.
- **Shader packs**: drop a shader in `UNI2-IM/Shaders` and pick it. It compiles when you select it
  and runs last. HLSL (`.hlsl`, `.ps`) compiles as is. `.fx`, `.slang`, `.glsl`, `.frag` and `.fsh`
  are translated first. Compiling needs `d3dcompiler_47.dll`, which comes with Windows and with
  Proton. Without it, the rest of the tab still works.

**Everything off** is a button on this tab and on Improvements. In one click it resets the present
size, all five stages, the upscale filter, back buffer multisampling, Character Visual Improvements,
the empty stage and POTATO MODE to the game's own. After that the mod draws nothing into the frame
and reads nothing from it.

## Installing a shader

1. Put the file in `<game folder>\UNI2-IM\Shaders`. Use the top of that folder, not a subfolder. The
   name does not matter, but the extension does.
2. In game, press F1 and go to **Graphics → Shaders**.
3. At **Shader pack**, press **Rescan** if the game was already running, then pick the file.
4. It compiles as soon as you pick it, and the line below tells you what happened:
   `my_scanlines.slang compiled and running - GLSL fragment stage, one pass`. Pick **Off** to stop.

Your pick is saved as `[Graphics] ShaderPack` and comes back on the next launch. Nothing is
installed, unpacked or registered. A shader is one file, and deleting the file removes it.

| Extension | Read as |
|---|---|
| `.hlsl` `.ps` | HLSL, compiled exactly as written |
| `.fx` | effect format, HLSL with annotations and techniques |
| `.slang` | Vulkan GLSL with `#pragma parameter`s |
| `.glsl` `.frag` `.fsh` | OpenGL GLSL, modern or legacy |

## Why they have to be translated

The game uses Direct3D 9. A shader that runs here must be HLSL compiled to pixel shader 3.0, and
there is one slot for it, over the finished frame.

A `.fx` file is HLSL, but it is not a pixel shader. It also describes uniforms, annotations,
textures, samplers and techniques made of passes, and all of that must be resolved before it can be
compiled. A `.slang` is Vulkan GLSL and a `.glsl` is OpenGL GLSL, and Direct3D cannot compile GLSL
at all.

So the mod rewrites them. Uniforms become their default values, samplers read the frame, the
resolution and time uniforms become the two constants below, and GLSL is rewritten as HLSL. The mod
does not ship a full effect runtime with a GLSL compiler, multi-pass rendering and its own render
targets.

**You get one pass over the finished frame, nothing more.** A shader that needs a second pass, a
lookup texture, the depth buffer or the previous frame will translate but look wrong. The big
multi-pass CRT shaders are like that. The single pass ones work.

The translated file is written to `UNI2-IM\Shaders\Translated\<file>.hlsl`, and that is the file the
compiler gets. When something fails, the tab shows the compiler's own error, and the line numbers
refer to that copy. Open it, read the `#define`s at the top to see what each uniform was replaced
with, fix it there, and save it into `Shaders` as your own `.hlsl`.

| Format | What it is given |
|---|---|
| `.fx` | `BUFFER_WIDTH`, `BUFFER_HEIGHT`, `BUFFER_RCP_WIDTH`, `BUFFER_RCP_HEIGHT`, `BUFFER_PIXEL_SIZE`, `BUFFER_SCREEN_SIZE`, `BUFFER_ASPECT_RATIO`, and the back buffer, pixel size and screen size symbols. Each `uniform` becomes its default value; one with a `source` of `timer` or `framecount` becomes `FrameTime`. Every sampler reads the frame, the depth buffer reads 1.0, and the first technique's `PixelShader` is the pass |
| `.slang` | `SourceSize`, `OriginalSize`, `OutputSize` and `FinalViewportSize` as `float4(w, h, 1/w, 1/h)`, plus `FrameCount`, `FrameDirection` and `MVP`. Each `#pragma parameter` becomes its default, the UBO and push constant blocks fold away, every sampler reads the frame, and the varying that carried the coordinate becomes the one this pass draws with. Only the fragment stage is kept |
| `.glsl` image shader | `iResolution`, `iTime`, `iTimeDelta`, `iFrame`, `iMouse`, `iDate` and `iChannel0` to `iChannel3`, with `fragCoord` in pixels and y up |
| plain GLSL | A sampler uniform becomes the frame, an `in` or `varying` becomes the coordinate, an `out` or `gl_FragColor` becomes the result, and a uniform whose name looks like a resolution or a time is answered from `FrameSize` and `FrameTime`. Everything else becomes 0. The legacy `#ifdef FRAGMENT` form is understood |

The rewrite works on the text, it is not a compiler. GLSL types and functions are mapped to their
HLSL names, single argument vector constructors become casts, and names HLSL reserves (`line`,
`sample`, `matrix`) are renamed.

## Writing a shader pack

The first time a new version starts, eighteen examples are written into `UNI2-IM/Shaders`, next to
a `README.txt` that repeats all of this.

Twelve are HLSL: `01_passthrough` (the skeleton to copy), then `02_grayscale`, `03_sepia`,
`04_invert`, `05_posterize`, `06_pixelate`, `07_chromatic`, `08_film_grain`, `09_vhs`, `10_lcd_grid`,
`11_outline` and `12_crt` (curvature, scanlines, a phosphor mask, bleed and vignette). Each keeps its
settings as `#define` lines at the top. Edit them and select the pack again to recompile.

The other six show one of each format the mod accepts, so you can compare each with what ends up in
the `Translated` folder: `13_reshade_tonemap.fx` (annotated uniforms, a sampler, a technique),
`14_slang_scanlines.slang` (`#pragma parameter`s, a UBO, two stages), `15_shadertoy_ripple.glsl` (one
`mainImage`), `16_bleach_bypass.ps` (HLSL under the other extension), `17_dot_matrix.frag` (modern
GLSL, `in`, `out`, `texture()`) and `18_bloom_glow.fsh` (old GLSL, `varying`, `gl_FragColor`,
`texture2D`).

Files you add or edit are never overwritten. A file you delete stays deleted until the mod updates.
The sources are in `resource/shaders/examples`.

A pack is one file: entry point `main`, target `ps_3_0`, one pass, pixel shader only. The mod gives
it three things and nothing else:

```hlsl
sampler2D Frame  : register(s0);   // the frame so far
float4 FrameSize : register(c0);   // xy = 1/width, 1/height   zw = width, height
float4 FrameTime : register(c1);   // x  = seconds since load  y = frames since load
```

No vertex shader of your own, no second pass, no copy of the previous frame, no depth.

`Frame` is sampled with **point** filtering, so reading it at `uv` is exact, which is what a pixel
art game wants. A pack that bends the coordinates has to do its own filtering. `12_crt.hlsl` has the
four tap bilinear code that does it.
