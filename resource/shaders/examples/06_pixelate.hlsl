// UNI2-Improvement-Mod shader pack: Pixelate
// Snaps the picture to a coarser grid. Raise BLOCK until it hurts.

#define BLOCK 4.0   // screen pixels per block

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float2 block = max(BLOCK, 1.0f) * FrameSize.xy;
    float2 snapped = (floor(uv / block) + 0.5f) * block;

    return float4(tex2D(Frame, snapped).rgb, 1.0f);
}
