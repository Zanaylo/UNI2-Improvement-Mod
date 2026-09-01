// UNI2-Improvement-Mod shader pack: libretro scanlines
//
// A libretro .slang, written the way RetroArch wants one: two stages behind #pragma stage, a UBO,
// #pragma parameter defaults and Vulkan style bindings. The mod keeps the fragment stage, turns
// each parameter into its default, points Source at the frame and rewrites the GLSL as HLSL. The
// vertex stage below is read and dropped - there is only ever one pass here.

#version 450

#pragma name scanlines

#pragma parameter SCANLINE_WEIGHT "Scanline darkness" 0.35 0.0 1.0 0.05
#pragma parameter SCANLINE_GAP "Lines per screen height" 360.0 120.0 1080.0 60.0
#pragma parameter MASK_WEIGHT "Aperture mask" 0.20 0.0 1.0 0.05
#pragma parameter BRIGHTNESS "Brightness back" 1.25 1.0 2.0 0.05

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
    vec4 SourceSize;
    vec4 OutputSize;
    uint FrameCount;
    float SCANLINE_WEIGHT;
    float SCANLINE_GAP;
    float MASK_WEIGHT;
    float BRIGHTNESS;
} global;

#pragma stage vertex

layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

vec3 Mask(vec2 position)
{
    float phase = fract(position.x / 3.0);
    vec3 tint = vec3(1.0, 1.0, 1.0);

    if (phase < 0.333)
        tint = vec3(1.0, 0.7, 0.7);
    else if (phase < 0.666)
        tint = vec3(0.7, 1.0, 0.7);
    else
        tint = vec3(0.7, 0.7, 1.0);

    return mix(vec3(1.0), tint, global.MASK_WEIGHT);
}

void main()
{
    vec3 colour = texture(Source, vTexCoord).rgb * global.BRIGHTNESS;

    float line = fract(vTexCoord.y * global.SCANLINE_GAP);
    float shade = 1.0 - global.SCANLINE_WEIGHT * pow(abs(line * 2.0 - 1.0), 1.5);

    colour *= shade;
    colour *= Mask(vTexCoord * global.SourceSize.xy);

    FragColor = vec4(clamp(colour, 0.0, 1.0), 1.0);
}
