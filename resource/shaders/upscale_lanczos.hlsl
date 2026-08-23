sampler2D Source : register(s0);

float4 SourceSize : register(c0);

float Weight(float x)
{
	float distance = abs(x);

	if (distance < 0.0001f)
		return 1.0f;

	if (distance >= 2.0f)
		return 0.0f;

	float scaled = 3.14159265f * distance;

	return (2.0f * sin(scaled) * sin(scaled * 0.5f)) / (scaled * scaled);
}

float3 Tap(float2 origin, float2 offset)
{
	return tex2D(Source, (origin + offset + 0.5f) * SourceSize.zw).rgb;
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float2 position = uv * SourceSize.xy - 0.5f;
	float2 origin = floor(position);
	float2 fraction = position - origin;

	float wx[4];
	float wy[4];

	for (int t = 0; t < 4; ++t)
	{
		float step = float(t) - 1.0f;
		wx[t] = Weight(step - fraction.x);
		wy[t] = Weight(step - fraction.y);
	}

	float3 colour = float3(0.0f, 0.0f, 0.0f);
	float total = 0.0f;

	for (int y = 0; y < 4; ++y)
	{
		for (int x = 0; x < 4; ++x)
		{
			float weight = wx[x] * wy[y];

			colour += Tap(origin, float2(float(x) - 1.0f, float(y) - 1.0f)) * weight;
			total += weight;
		}
	}

	return float4(colour / max(total, 0.0001f), tex2D(Source, uv).a);
}
