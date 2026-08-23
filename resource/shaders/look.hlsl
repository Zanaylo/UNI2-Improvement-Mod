sampler2D Frame : register(s0);

float4 Levels : register(c0);
float4 Tint : register(c1);
float4 Screen : register(c2);

float Luma(float3 colour)
{
	return dot(colour, float3(0.2126f, 0.7152f, 0.0722f));
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float3 colour = tex2D(Frame, uv).rgb;

	colour = saturate(colour + Levels.x);
	colour = saturate((colour - 0.5f) * (1.0f + Levels.y) + 0.5f);
	colour = pow(max(colour, 0.0001f), Levels.z);

	float luma = Luma(colour);
	colour = saturate(lerp(luma.xxx, colour, 1.0f + Levels.w));

	float chroma = max(colour.r, max(colour.g, colour.b)) -
		min(colour.r, min(colour.g, colour.b));
	colour = saturate(lerp(luma.xxx, colour, 1.0f + Tint.x * (1.0f - chroma)));

	colour.r = saturate(colour.r + Tint.y * 0.08f);
	colour.b = saturate(colour.b - Tint.y * 0.08f);

	float2 centred = uv - 0.5f;
	colour *= 1.0f - Tint.z * saturate(dot(centred, centred) * 2.0f);

	float scanline = step(0.5f, frac(uv.y * Screen.y * 0.5f));
	colour *= 1.0f - Tint.w * 0.5f * scanline;

	float noise = frac(52.9829189f *
		frac(dot(uv * Screen.xy, float2(0.06711056f, 0.00583715f))));
	colour += Screen.z * (noise - 0.5f) * (1.0f / 255.0f);

	return float4(saturate(colour), 1.0f);
}
