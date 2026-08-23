sampler2D Frame : register(s0);

float4 SourceSize : register(c0);
float4 Tuning : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float2 texel = SourceSize.xy;

	float3 colour = tex2D(Frame, uv + float2(-texel.x, -texel.y)).rgb;
	colour += tex2D(Frame, uv + float2(texel.x, -texel.y)).rgb;
	colour += tex2D(Frame, uv + float2(-texel.x, texel.y)).rgb;
	colour += tex2D(Frame, uv + float2(texel.x, texel.y)).rgb;
	colour *= 0.25f;

	float luma = max(colour.r, max(colour.g, colour.b));
	float knee = max(Tuning.y, 0.0001f);

	float soft = saturate(luma - Tuning.x + knee);
	soft = soft * soft / (4.0f * knee);

	float weight = max(soft, luma - Tuning.x) / max(luma, 0.0001f);

	return float4(colour * weight, 1.0f);
}
