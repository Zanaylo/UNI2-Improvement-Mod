// UNI2-Improvement-Mod shader pack: ReShade tone map
//
// A ReShade .fx, written the way ReShade wants one: an include, annotated uniforms, a sampler of
// its own and a technique. The mod translates it on the way in - the uniforms become their default
// values, the sampler becomes the frame, and the technique's PixelShader becomes the pass. Pick it
// on the Shaders tab and read Translated\13_reshade_tonemap.fx.hlsl to see what it turned into.

#include "ReShade.fxh"

uniform float Exposure <
    ui_type = "slider";
    ui_min = 0.2; ui_max = 3.0;
    ui_label = "Exposure";
> = 1.15;

uniform float Contrast <
    ui_type = "slider";
    ui_min = 0.5; ui_max = 2.0;
    ui_label = "Contrast";
> = 1.08;

uniform float Saturation <
    ui_type = "slider";
    ui_min = 0.0; ui_max = 2.0;
    ui_label = "Saturation";
> = 1.10;

uniform float Bleach <
    ui_type = "slider";
    ui_min = 0.0; ui_max = 1.0;
    ui_label = "Highlight roll off";
> = 0.75;

sampler Source
{
    Texture = ReShade::BackBufferTex;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

float3 Reinhard(float3 colour, float white)
{
    return colour * (1.0 + colour / (white * white)) / (1.0 + colour);
}

void PS_ToneMap(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 result : SV_Target)
{
    float3 colour = tex2D(Source, texcoord).rgb * Exposure;

    colour = Reinhard(colour, lerp(4.0, 1.0, Bleach));
    colour = (colour - 0.5) * Contrast + 0.5;

    float luma = dot(colour, float3(0.2126, 0.7152, 0.0722));
    colour = lerp(luma.xxx, colour, Saturation);

    float2 centred = texcoord - 0.5;
    float vignette = 1.0 - dot(centred, centred) * 0.45;

    result = float4(saturate(colour * vignette), 1.0);
}

technique ToneMap
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_ToneMap;
    }
}
