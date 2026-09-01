// UNI2-Improvement-Mod shader pack: Chromatic aberration
// Red and blue pull apart towards the corners, the way a cheap lens does.

#define AMOUNT 2.5   // screen pixels of split at the corner
#define FALLOFF 1.0  // 1 = grows with distance from the centre, 0 = the same everywhere

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
