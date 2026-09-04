# Shaders

Its own tab, next to Improvements. Everything on it runs over the picture. None of it touches the
simulation, the inputs, or anything an opponent can see.

**Every stage is off by default and every one has an explicit Off.** While they all are, the chain
does not read the back buffer at all and the frame is the game's own.

- **Upscale filter** — Off, bicubic, Lanczos or FidelityFX EASU, in place of the engine's bilinear
  stretch to your window. EASU reads the gradient of the neighbourhood and stretches its kernel along
  the edge it finds, turning a diagonal back into a line instead of a staircase. Needs a back buffer
  larger than 1280x720, so raise the present size with it.
- **Anti-aliasing** — FXAA in five steps. Multisampling is not offered because it cannot reach this
  game: a Direct3D 9 texture cannot be multisampled and the whole scene is drawn into textures.
- **Bloom** — the bright parts of the frame cut out, blurred at a quarter size and screened back on.
  The one setting here that reads as lighting instead of filtering. The neon in these stages and the
  glow on EXS and super effects are what it is for.
- **Sharpening** — contrast adaptive, or FidelityFX RCAS to pair with the EASU upscale.
- **Colour and display** — brightness, contrast, gamma, saturation, vibrance, warmth, vignette,
  scanlines and dither.
- **Shader packs** — drop a shader in `UNI2-IM/Shaders` and pick it. It compiles on selection and
  runs last in the chain. HLSL (`.hlsl`, `.ps`) compiles as it stands; `.fx`, `.slang`, `.glsl`,
  `.frag` and `.fsh` are translated on the way in. Compiling needs `d3dcompiler_47.dll`, which ships
  with Windows and with Proton. Without it the rest of the tab still works.

**Everything off** is a button on this tab and on Improvements. It puts the present size, all five
stages, the upscale filter, back buffer multisampling, Character Visual Improvements, the empty stage
and POTATO MODE back to the game's own in one click. After it the mod draws nothing into the frame
and reads nothing out of it.

## Installing a shader

1. Drop the file in `<game folder>\UNI2-IM\Shaders`. The top of that folder, not a subfolder. The
   name does not matter, the extension does.
2. In game, press F1 and go to **Graphics → Shaders**.
3. At **Shader pack**, press **Rescan** if the game was already running, then pick the file.
4. It compiles the moment you pick it, and the line underneath says what happened:
   `my_scanlines.slang compiled and running - GLSL fragment stage, one pass`. Pick **Off** to stop.

The pick is saved as `[Graphics] ShaderPack` and comes back next launch. Nothing is installed,
unpacked or registered. A shader is one file, and deleting it removes it.

| Extension | Read as |
|---|---|
| `.hlsl` `.ps` | HLSL, compiled exactly as written |
| `.fx` | effect format, HLSL with annotations and techniques |
| `.slang` | Vulkan GLSL with `#pragma parameter`s |
| `.glsl` `.frag` `.fsh` | OpenGL GLSL, modern or legacy |

## Why they have to be translated

The game is Direct3D 9. A shader that runs here has to be HLSL compiled to pixel shader 3.0, and
there is one slot for it, over the finished frame.

A `.fx` is HLSL but it is not a pixel shader. It is a small program describing uniforms,
annotations, textures, samplers and techniques made of passes, and something has to resolve that
before a compiler sees it. A `.slang` is Vulkan GLSL and a `.glsl` is OpenGL GLSL, and Direct3D
cannot compile GLSL at all.

So the mod rewrites them: uniforms become their default values, samplers become the frame, the
resolution and time uniforms become the two constants below, and the GLSL is rewritten as HLSL. The
alternative is shipping a full effect runtime, with a GLSL compiler, multi-pass rendering and its
own render targets. That is not a training mod.

**One pass over the finished frame is the whole budget.** A shader that wants a second pass, a lookup
texture, the depth buffer or the previous frame will translate and then be wrong. The big multi-pass
CRT shaders are exactly that. The single pass ones work.

The translation is written to `UNI2-IM\Shaders\Translated\<file>.hlsl`, and that file is what the
compiler was given. When something fails, the tab prints the compiler's own error and says the line
numbers belong to that copy. Open it, read the block of `#define`s at the top to see what each
uniform was replaced with, fix it there, and save the result into `Shaders` as an `.hlsl` of your
own.

| Format | What it is given |
|---|---|
| `.fx` | `BUFFER_WIDTH`, `BUFFER_HEIGHT`, `BUFFER_RCP_WIDTH`, `BUFFER_RCP_HEIGHT`, `BUFFER_PIXEL_SIZE`, `BUFFER_SCREEN_SIZE`, `BUFFER_ASPECT_RATIO`, and the back buffer, pixel size and screen size symbols. Each `uniform` becomes its default value; one with a `source` of `timer` or `framecount` becomes `FrameTime`. Every sampler reads the frame, the depth buffer reads 1.0, and the first technique's `PixelShader` is the pass |
| `.slang` | `SourceSize`, `OriginalSize`, `OutputSize` and `FinalViewportSize` as `float4(w, h, 1/w, 1/h)`, plus `FrameCount`, `FrameDirection` and `MVP`. Each `#pragma parameter` becomes its default, the UBO and push constant blocks fold away, every sampler reads the frame, and the varying that carried the coordinate becomes the one this pass draws with. Only the fragment stage is kept |
| `.glsl` image shader | `iResolution`, `iTime`, `iTimeDelta`, `iFrame`, `iMouse`, `iDate` and `iChannel0` to `iChannel3`, with `fragCoord` in pixels and y up |
| plain GLSL | A sampler uniform becomes the frame, an `in` or `varying` becomes the coordinate, an `out` or `gl_FragColor` becomes the result, and a uniform whose name looks like a resolution or a time is answered from `FrameSize` and `FrameTime`. Everything else becomes 0. The legacy `#ifdef FRAGMENT` form is understood |

The rewrite is lexical, not a compiler. GLSL types and intrinsics are mapped to their HLSL names,
single argument vector constructors become casts, and identifiers HLSL reserves (`line`, `sample`,
`matrix`) are renamed.

## Writing a shader pack

Eighteen examples are written into `UNI2-IM/Shaders` the first time a new version starts, next to a
`README.txt` that repeats all of this.

Twelve are HLSL: `01_passthrough` (the skeleton to copy), then `02_grayscale`, `03_sepia`,
`04_invert`, `05_posterize`, `06_pixelate`, `07_chromatic`, `08_film_grain`, `09_vhs`, `10_lcd_grid`,
`11_outline` and `12_crt` — curvature, scanlines, a phosphor mask, bleed and vignette. Each keeps its
settings as `#define` lines at the top. Edit them and reselect the pack to compile again.

The other six are one per format the mod takes, so you can read each next to what the `Translated`
folder makes of it: `13_reshade_tonemap.fx` (annotated uniforms, a sampler, a technique),
`14_slang_scanlines.slang` (`#pragma parameter`s, a UBO, two stages), `15_shadertoy_ripple.glsl` (one
`mainImage`), `16_bleach_bypass.ps` (HLSL under the other extension), `17_dot_matrix.frag` (modern
GLSL, `in`, `out`, `texture()`) and `18_bloom_glow.fsh` (old GLSL, `varying`, `gl_FragColor`,
`texture2D`).

Files you add or edit are never overwritten, and a file you delete stays deleted until the mod
updates. The sources are in `resource/shaders/examples`.

A pack is one file: entry point `main`, target `ps_3_0`, one pass, pixel shader only. The mod binds
three things and nothing else:

```hlsl
sampler2D Frame  : register(s0);   // the frame so far
float4 FrameSize : register(c0);   // xy = 1/width, 1/height   zw = width, height
float4 FrameTime : register(c1);   // x  = seconds since load  y = frames since load
```

No vertex shader of yours, no second pass, no copy of the previous frame, no depth.

`Frame` is sampled with **point** filtering, so reading straight through at `uv` is exact, which is
what a pixel art game wants. A pack that bends the coordinates has to filter for itself.
`12_crt.hlsl` carries the four tap bilinear that does it.
