sampler2D Art : register(s0);

float4 Texel : register(c0);

float4 Tap(float2 origin, float2 offset)
{
	float4 texel = tex2Dlod(Art, float4((origin + offset + 0.5f) * Texel.xy, 0.0f, 0.0f));
	texel.rgb *= texel.a;
	return texel;
}

float Edge(float4 texel)
{
	return texel.g * 0.5f + texel.a * 0.5f;
}

void Analyse(inout float2 dir, inout float len, float weight, float a, float b, float c, float d, float e)
{
	float horizontal = d - b;
	float lenX = saturate(abs(horizontal) * rcp(max(max(abs(d - c), abs(c - b)), 0.000001f)));

	dir.x += horizontal * weight;
	len += lenX * lenX * weight;

	float vertical = e - a;
	float lenY = saturate(abs(vertical) * rcp(max(max(abs(e - c), abs(c - a)), 0.000001f)));

	dir.y += vertical * weight;
	len += lenY * lenY * weight;
}

void Accumulate(inout float4 colour, inout float weight, float2 offset, float2 dir, float2 len, float lob,
	float clip, float4 tap)
{
	float2 rotated = float2(offset.x * dir.x + offset.y * dir.y, offset.x * -dir.y + offset.y * dir.x) * len;
	float distance = min(dot(rotated, rotated), clip);

	float base = 0.4f * distance - 1.0f;
	float window = lob * distance - 1.0f;

	base *= base;
	window *= window;

	float amount = (1.5625f * base - 0.5625f) * window;

	colour += tap * amount;
	weight += amount;
}

float4 Upscale(float2 uv)
{
	float2 position = uv * Texel.zw - 0.5f;
	float2 origin = floor(position);
	float2 fraction = position - origin;

	float4 b = Tap(origin, float2(0.0f, -1.0f));
	float4 c = Tap(origin, float2(1.0f, -1.0f));
	float4 e = Tap(origin, float2(-1.0f, 0.0f));
	float4 f = Tap(origin, float2(0.0f, 0.0f));
	float4 g = Tap(origin, float2(1.0f, 0.0f));
	float4 h = Tap(origin, float2(2.0f, 0.0f));
	float4 i = Tap(origin, float2(-1.0f, 1.0f));
	float4 j = Tap(origin, float2(0.0f, 1.0f));
	float4 k = Tap(origin, float2(1.0f, 1.0f));
	float4 l = Tap(origin, float2(2.0f, 1.0f));
	float4 n = Tap(origin, float2(0.0f, 2.0f));
	float4 o = Tap(origin, float2(1.0f, 2.0f));

	float2 dir = float2(0.0f, 0.0f);
	float len = 0.0f;

	Analyse(dir, len, (1.0f - fraction.x) * (1.0f - fraction.y), Edge(b), Edge(e), Edge(f), Edge(g), Edge(j));
	Analyse(dir, len, fraction.x * (1.0f - fraction.y), Edge(c), Edge(f), Edge(g), Edge(h), Edge(k));
	Analyse(dir, len, (1.0f - fraction.x) * fraction.y, Edge(f), Edge(i), Edge(j), Edge(k), Edge(n));
	Analyse(dir, len, fraction.x * fraction.y, Edge(g), Edge(j), Edge(k), Edge(l), Edge(o));

	float square = dot(dir, dir);
	bool flat = square < (1.0f / 32768.0f);

	dir.x = flat ? 1.0f : dir.x;
	dir *= flat ? 1.0f : rsqrt(max(square, 0.000001f));

	len = len * 0.5f;
	len *= len;

	float stretch = rcp(max(max(abs(dir.x), abs(dir.y)), 0.000001f));
	float2 extent = float2(1.0f + (stretch - 1.0f) * len, 1.0f - 0.5f * len);
	float lob = 0.5f - 0.29f * len;
	float clip = rcp(lob);

	float4 lowest = min(min(f, g), min(j, k));
	float4 highest = max(max(f, g), max(j, k));

	float4 colour = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float weight = 0.0f;

	Accumulate(colour, weight, float2(0.0f, -1.0f) - fraction, dir, extent, lob, clip, b);
	Accumulate(colour, weight, float2(1.0f, -1.0f) - fraction, dir, extent, lob, clip, c);
	Accumulate(colour, weight, float2(-1.0f, 0.0f) - fraction, dir, extent, lob, clip, e);
	Accumulate(colour, weight, float2(0.0f, 0.0f) - fraction, dir, extent, lob, clip, f);
	Accumulate(colour, weight, float2(1.0f, 0.0f) - fraction, dir, extent, lob, clip, g);
	Accumulate(colour, weight, float2(2.0f, 0.0f) - fraction, dir, extent, lob, clip, h);
	Accumulate(colour, weight, float2(-1.0f, 1.0f) - fraction, dir, extent, lob, clip, i);
	Accumulate(colour, weight, float2(0.0f, 1.0f) - fraction, dir, extent, lob, clip, j);
	Accumulate(colour, weight, float2(1.0f, 1.0f) - fraction, dir, extent, lob, clip, k);
	Accumulate(colour, weight, float2(2.0f, 1.0f) - fraction, dir, extent, lob, clip, l);
	Accumulate(colour, weight, float2(0.0f, 2.0f) - fraction, dir, extent, lob, clip, n);
	Accumulate(colour, weight, float2(1.0f, 2.0f) - fraction, dir, extent, lob, clip, o);

	float4 pixel = min(highest, max(lowest, colour * rcp(weight)));
	pixel.rgb *= rcp(max(pixel.a, 0.000001f));

	return pixel;
}

float4 main(float2 uv : TEXCOORD0, float4 diffuse : COLOR0) : COLOR0
{
	return Upscale(uv) * diffuse;
}
