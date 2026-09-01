// UNI2-Improvement-Mod shader pack: Outline
// A Sobel edge detector inking the picture, for a comic book look.

#define THICKNESS 1.0  // screen pixels between the taps
#define STRENGTH 1.0   // how black the ink goes
#define THRESHOLD 0.12 // edges weaker than this are ignored
#define FLATTEN 0.0    // 1 = ink on a white page, 0 = ink over the game

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float Luma(float2 uv)
{
    return dot(tex2D(Frame, uv).rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float3 colour = tex2D(Frame, uv).rgb;

    float2 tap = THICKNESS * FrameSize.xy;

    float tl = Luma(uv + float2(-tap.x, -tap.y));
    float tc = Luma(uv + float2(0.0f, -tap.y));
    float tr = Luma(uv + float2(tap.x, -tap.y));
    float ml = Luma(uv + float2(-tap.x, 0.0f));
    float mr = Luma(uv + float2(tap.x, 0.0f));
    float bl = Luma(uv + float2(-tap.x, tap.y));
    float bc = Luma(uv + float2(0.0f, tap.y));
    float br = Luma(uv + float2(tap.x, tap.y));

    float gx = (tr + 2.0f * mr + br) - (tl + 2.0f * ml + bl);
    float gy = (bl + 2.0f * bc + br) - (tl + 2.0f * tc + tr);

    float edge = saturate(sqrt(gx * gx + gy * gy) - THRESHOLD);
    float3 page = lerp(colour, float3(1.0f, 1.0f, 1.0f), FLATTEN);

    return float4(saturate(page * (1.0f - edge * STRENGTH)), 1.0f);
}
