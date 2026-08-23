sampler2D Frame : register(s0);

float4 TexelSize : register(c0);
float4 Strength : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float2 texel = TexelSize.xy;

	float3 b = tex2D(Frame, uv + float2(0.0f, -texel.y)).rgb;
	float3 d = tex2D(Frame, uv + float2(-texel.x, 0.0f)).rgb;
	float3 e = tex2D(Frame, uv).rgb;
	float3 f = tex2D(Frame, uv + float2(texel.x, 0.0f)).rgb;
	float3 h = tex2D(Frame, uv + float2(0.0f, texel.y)).rgb;

	float3 lowest = min(min(b, d), min(f, h));
	float3 highest = max(max(b, d), max(f, h));

	float3 hitLow = lowest / max(4.0f * highest, 0.0001f);
	float3 hitHigh = (1.0f - highest) / min(4.0f * lowest - 4.0f, -0.0001f);

	float3 lobeRgb = max(-hitLow, hitHigh);
	float peak = max(lobeRgb.r, max(lobeRgb.g, lobeRgb.b));

	float sharp = exp2(-lerp(2.0f, 0.0f, saturate(Strength.x)));
	float lobe = max(-0.1875f, min(peak, 0.0f)) * sharp;

	return float4((lobe * (b + d + f + h) + e) / (4.0f * lobe + 1.0f), 1.0f);
}
