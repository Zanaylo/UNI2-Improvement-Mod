// UNI2-Improvement-Mod shader pack: LCD grid
// The dot grid of a handheld screen, with the flat wash of a cheap panel.

#define CELL 3.0        // screen pixels per LCD dot
#define GRID_DEPTH 0.35 // how dark the gaps between dots are
#define WASH 0.15       // 0 = the colour as it was, 1 = fully washed out

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float size = max(CELL, 2.0f);
    float2 cell = size * FrameSize.xy;
    float2 snapped = (floor(uv / cell) + 0.5f) * cell;

    float3 colour = tex2D(Frame, snapped).rgb;

    float2 inside = frac(uv / cell);
    float2 gap = step(inside, 1.0f / size);

    colour *= 1.0f - GRID_DEPTH * saturate(gap.x + gap.y);

    float luma = dot(colour, float3(0.2126f, 0.7152f, 0.0722f));

    colour = lerp(colour, lerp(colour, luma.xxx, 0.5f) * 0.9f + 0.08f, WASH);

    return float4(saturate(colour), 1.0f);
}
