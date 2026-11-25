#define AMOUNT 2.5
#define FALLOFF 1.0

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float2 fromCentre = uv - 0.5f;
    float weight = lerp(1.0f, length(fromCentre) * 2.0f, FALLOFF);
    float2 shift = fromCentre * weight * AMOUNT * FrameSize.xy;

    float red = tex2D(Frame, uv + shift).r;
    float green = tex2D(Frame, uv).g;
    float blue = tex2D(Frame, uv - shift).b;

    return float4(red, green, blue, 1.0f);
}
