sampler2D Upscaled : register(s0);
sampler2D Detailed : register(s1);
sampler2D Final : register(s2);
sampler2D Snapshot : register(s3);

float4 Settings : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float3 upscaled = tex2D(Upscaled, uv).rgb;
	float3 stage = tex2D(Snapshot, uv).rgb;
	float3 relative = abs(tex2D(Final, uv).rgb - stage) / max(stage, Settings.y);
	float visible = 1.0 - saturate(max(relative.r, max(relative.g, relative.b)) * Settings.x);

	return float4(lerp(upscaled, tex2D(Detailed, uv).rgb, visible), 1.0);
}
