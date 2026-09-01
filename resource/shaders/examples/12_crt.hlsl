// UNI2-Improvement-Mod shader pack: CRT
// Drop in UNI2-IM/Shaders and pick it in the Shaders tab.
// Everything you may want to change is in this block.

#define CURVATURE      0.55   // 0 = flat glass, 1.5 = fishbowl
#define SCANLINE_PITCH 2.0    // screen pixels per scanline. 2 at 720p, 3 or 4 on a big window
#define SCANLINE_DEPTH 0.35   // 0 = none, 1 = black between lines
#define MASK_STRENGTH  0.30   // phosphor stripes. 0 = none, 1 = hard RGB
#define BLEED          0.35   // horizontal smear, the CRT's soft focus
#define VIGNETTE       0.25   // corner darkening
#define BRIGHTNESS     1.25   // put back the light the mask and lines take away
#define BORDER         0.0    // 1 = black outside the curved glass, 0 = stretch to fill

sampler2D Frame : register(s0);
float4 FrameSize : register(c0);
float4 FrameTime : register(c1);

float2 Curve(float2 uv)
{
    float2 c = uv * 2.0f - 1.0f;
    float2 bend = abs(c.yx) / float2(6.0f, 5.0f);
    c += c * bend * bend * CURVATURE;
    return c * 0.5f + 0.5f;
}

float3 SampleFrame(float2 uv)
{
    float2 p = uv * FrameSize.zw - 0.5f;
    float2 f = frac(p);
    float2 base = (floor(p) + 0.5f) * FrameSize.xy;

    float3 a = tex2D(Frame, base).rgb;
    float3 b = tex2D(Frame, base + float2(FrameSize.x, 0.0f)).rgb;
    float3 c = tex2D(Frame, base + float2(0.0f, FrameSize.y)).rgb;
    float3 d = tex2D(Frame, base + FrameSize.xy).rgb;

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float3 Bleed(float2 uv)
{
    float3 centre = SampleFrame(uv);

    if (BLEED <= 0.0f)
        return centre;

    float2 offset = float2(FrameSize.x, 0.0f);
    float3 left = SampleFrame(uv - offset);
    float3 right = SampleFrame(uv + offset);

    return lerp(centre, centre * 0.5f + (left + right) * 0.25f, BLEED);
}

float3 Mask(float2 pixel)
{
    float dim = 1.0f - MASK_STRENGTH;
    float cell = fmod(floor(pixel.x), 3.0f);

    float3 mask = float3(dim, dim, 1.0f);

    if (cell < 1.0f)
        mask = float3(1.0f, dim, dim);
    else if (cell < 2.0f)
        mask = float3(dim, 1.0f, dim);

    return mask;
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float2 warped = Curve(uv);

    if (BORDER > 0.0f)
    {
        float2 outside = max(-warped, warped - 1.0f);

        if (max(outside.x, outside.y) > 0.0f)
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    warped = saturate(warped);

    float3 colour = Bleed(warped);

    float2 pixel = uv * FrameSize.zw;

    colour *= 1.0f - SCANLINE_DEPTH * step(0.5f, frac(pixel.y / SCANLINE_PITCH));
    colour *= Mask(pixel);

    float2 edge = warped * (1.0f - warped) * 4.0f;
    colour *= lerp(1.0f, pow(saturate(edge.x * edge.y), 0.25f), VIGNETTE);

    return float4(saturate(colour * BRIGHTNESS), 1.0f);
}
