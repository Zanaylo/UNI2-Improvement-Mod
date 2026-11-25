#define WOBBLE 1.5
#define WOBBLE_BANDS 90.0
#define WOBBLE_SPEED 2.0
#define SPLIT 2.0
#define BAND_SPEED 0.15
#define BAND_SIZE 0.03
#define NOISE 0.05

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float time = FrameTime.x;

    float wave = sin(uv.y * WOBBLE_BANDS + time * WOBBLE_SPEED);
    float band = frac(uv.y + time * BAND_SPEED);
    float inBand = step(band, BAND_SIZE);

    float2 shifted = uv;
    shifted.x += (wave * WOBBLE + inBand * WOBBLE * 6.0f) * FrameSize.x;

    float2 split = float2(SPLIT * FrameSize.x, 0.0f);

    float red = tex2D(Frame, shifted + split).r;
    float green = tex2D(Frame, shifted).g;
    float blue = tex2D(Frame, shifted - split).b;

    float3 colour = float3(red, green, blue);

    float noise = Hash(uv * FrameSize.zw + frac(time) * 100.0f) - 0.5f;

    colour += noise * NOISE;
    colour += inBand * 0.12f;

    return float4(saturate(colour), 1.0f);
}
