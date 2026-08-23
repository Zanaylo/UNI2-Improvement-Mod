sampler2D Frame : register(s0);
sampler2D Bloom : register(s1);

float4 Tuning : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float3 scene = tex2D(Frame, uv).rgb;
	float3 glow = saturate(tex2D(Bloom, uv).rgb * Tuning.x);

	return float4(1.0f - (1.0f - scene) * (1.0f - glow), 1.0f);
}
