// UNI2-Improvement-Mod shader pack: Film grain
// Moving noise, and the first pack here that uses FrameTime.

#define GRAIN 0.06     // 0 = none, 0.2 = a bad print
#define IN_SHADOWS 1.0 // 1 = grain sits mostly in the dark parts, 0 = evenly everywhere

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float3 colour = tex2D(Frame, uv).rgb;

    float noise = Hash(uv * FrameSize.zw + frac(FrameTime.x) * 100.0f) - 0.5f;
    float luma = dot(colour, float3(0.2126f, 0.7152f, 0.0722f));
    float weight = lerp(1.0f, 1.0f - luma, IN_SHADOWS);

    return float4(saturate(colour + noise * GRAIN * weight), 1.0f);
}
