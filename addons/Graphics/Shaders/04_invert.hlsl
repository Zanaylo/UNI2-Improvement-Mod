#define STRENGTH 1.0

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float3 colour = tex2D(Frame, uv).rgb;

    return float4(lerp(colour, 1.0f - colour, STRENGTH), 1.0f);
}
