// UNI2-Improvement-Mod shader pack: Grayscale
// Black and white, by the weights the eye actually uses.

#define STRENGTH 1.0   // 0 = untouched, 1 = fully grey

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float3 colour = tex2D(Frame, uv).rgb;
    float luma = dot(colour, float3(0.2126f, 0.7152f, 0.0722f));

    return float4(lerp(colour, luma.xxx, STRENGTH), 1.0f);
}
