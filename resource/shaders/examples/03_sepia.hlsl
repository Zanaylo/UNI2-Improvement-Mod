// UNI2-Improvement-Mod shader pack: Sepia
// Grayscale with an old paper tint on it.

#define STRENGTH 0.9                    // 0 = untouched, 1 = full sepia
#define TINT float3(1.07f, 0.82f, 0.58f) // the paper colour

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float3 colour = tex2D(Frame, uv).rgb;
    float luma = dot(colour, float3(0.2126f, 0.7152f, 0.0722f));

    return float4(saturate(lerp(colour, luma * TINT, STRENGTH)), 1.0f);
}
