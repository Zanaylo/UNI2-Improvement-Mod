sampler2D Source : register(s0);

float4 SourceSize : register(c0);

float4 Tap(float2 position)
{
	return tex2D(Source, position * SourceSize.zw);
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float2 position = uv * SourceSize.xy;
	float2 centre = floor(position - 0.5f) + 0.5f;
	float2 f = position - centre;

	float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
	float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
	float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
	float2 w3 = f * f * (-0.5f + 0.5f * f);

	float2 w12 = w1 + w2;
	float2 offset12 = w2 / max(w12, 0.0001f);

	float2 near = centre - 1.0f;
	float2 far = centre + 2.0f;
	float2 middle = centre + offset12;

	float4 colour = float4(0.0f, 0.0f, 0.0f, 0.0f);

	colour += Tap(float2(near.x, near.y)) * (w0.x * w0.y);
	colour += Tap(float2(middle.x, near.y)) * (w12.x * w0.y);
	colour += Tap(float2(far.x, near.y)) * (w3.x * w0.y);

	colour += Tap(float2(near.x, middle.y)) * (w0.x * w12.y);
	colour += Tap(float2(middle.x, middle.y)) * (w12.x * w12.y);
	colour += Tap(float2(far.x, middle.y)) * (w3.x * w12.y);

	colour += Tap(float2(near.x, far.y)) * (w0.x * w3.y);
	colour += Tap(float2(middle.x, far.y)) * (w12.x * w3.y);
	colour += Tap(float2(far.x, far.y)) * (w3.x * w3.y);

	return colour;
}
