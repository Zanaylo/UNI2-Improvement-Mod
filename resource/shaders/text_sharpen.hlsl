sampler2D Page : register(s0);

float4 Texel : register(c0);
float4 Edge : register(c1);

float4 Smooth(float2 uv)
{
	float2 position = uv * Texel.zw - 0.5f;
	float2 cell = floor(position);
	float2 f = position - cell;
	float2 f2 = f * f;
	float2 f3 = f2 * f;

	float2 w0 = (1.0f - 3.0f * f + 3.0f * f2 - f3) / 6.0f;
	float2 w1 = (4.0f - 6.0f * f2 + 3.0f * f3) / 6.0f;
	float2 w2 = (1.0f + 3.0f * f + 3.0f * f2 - 3.0f * f3) / 6.0f;
	float2 w3 = f3 / 6.0f;

	float2 g0 = w0 + w1;
	float2 g1 = w2 + w3;
	float2 h0 = (cell - 0.5f + w1 / g0) * Texel.xy;
	float2 h1 = (cell + 1.5f + w3 / g1) * Texel.xy;

	float4 top = tex2D(Page, float2(h0.x, h0.y)) * g0.x + tex2D(Page, float2(h1.x, h0.y)) * g1.x;
	float4 bottom = tex2D(Page, float2(h0.x, h1.y)) * g0.x + tex2D(Page, float2(h1.x, h1.y)) * g1.x;

	return top * g0.y + bottom * g1.y;
}

float4 main(float2 uv : TEXCOORD0, float4 diffuse : COLOR0) : COLOR0
{
	float4 page = saturate((Smooth(uv) - 0.5f) * Edge.x + 0.5f);
	return page * diffuse;
}
