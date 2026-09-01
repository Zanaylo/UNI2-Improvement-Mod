// UNI2-Improvement-Mod shader pack: Posterize
// Cuts the colour down to a few steps per channel, the way an old palette did.

#define LEVELS 8.0   // steps per channel. 2 is a poster, 32 is nearly untouched
#define DITHER 1.0   // 0 = hard bands, 1 = a 2x2 pattern to break them up

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float3 colour = tex2D(Frame, uv).rgb;

    float2 cell = fmod(floor(uv * FrameSize.zw), 2.0f);
    float bayer = cell.x * 0.5f + cell.y * 0.25f;
    float offset = (bayer - 0.375f) * DITHER;

    float steps = max(LEVELS - 1.0f, 1.0f);

    return float4(saturate(floor(colour * steps + 0.5f + offset) / steps), 1.0f);
}
