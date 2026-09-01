// UNI2-Improvement-Mod shader pack: Dot matrix
//
// A modern GLSL fragment shader - the shape a .frag from a GL renderer or a shader editor has:
// a sampler uniform, an `in` varying carrying the coordinate, an `out vec4`, and a void main. The
// mod points the sampler at the frame, turns the varying into the coordinate it draws with, and
// rewrites the GLSL as HLSL.

#version 330 core

uniform sampler2D uScreen;
uniform vec2 uScreenSize;

in vec2 vTexCoord;
out vec4 FragColor;

const float CELL = 3.0;
const float GAP = 0.28;
const float LEVELS = 4.0;

const vec3 INK = vec3(0.06, 0.22, 0.10);
const vec3 PAPER = vec3(0.55, 0.75, 0.30);

float Cell(vec2 pixel)
{
    vec2 inside = fract(pixel / CELL) - 0.5;
    float distance = length(inside) * 2.0;

    return 1.0 - smoothstep(1.0 - GAP, 1.0, distance);
}

void main()
{
    vec2 pixel = vTexCoord * uScreenSize;
    vec2 snapped = floor(pixel / CELL) * CELL / uScreenSize;

    vec3 colour = texture(uScreen, snapped).rgb;
    float luma = dot(colour, vec3(0.2126, 0.7152, 0.0722));

    float stepped = floor(luma * LEVELS + 0.5) / LEVELS;
    vec3 tinted = mix(INK, PAPER, stepped);

    FragColor = vec4(tinted * Cell(pixel), 1.0);
}
