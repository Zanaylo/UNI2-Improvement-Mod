// AMD FidelityFX EASU over the 1280x720 scene target, drawn into a back buffer sized copy.
//
// The engine composites its five scene targets into the back buffer with a linear filter, and that
// magnification is the only one in the whole frame - the sprites land in the targets at about 1:1.
// An edge directed kernel earns its keep exactly there and nowhere else, so this runs before the
// composite rather than over the finished frame.
//
// c0.xy is the source size in texels and c0.zw its reciprocal.

sampler2D Scene : register(s0);

float4 SourceSize : register(c0);

float3 Tap(float2 base, float2 offset)
{
	return tex2D(Scene, (base + offset + 0.5f) * SourceSize.zw).rgb;
}

void Analyse(inout float2 dir, inout float len, float weight, float a, float b, float c, float d,
	float e)
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

void Accumulate(inout float3 colour, inout float weight, float2 offset, float2 dir, float2 len,
	float lob, float clip, float3 tap)
{
	float2 rotated = float2(offset.x * dir.x + offset.y * dir.y,
		offset.x * -dir.y + offset.y * dir.x) * len;

	float distance = min(dot(rotated, rotated), clip);

	float base = 0.4f * distance - 1.0f;
	float window = lob * distance - 1.0f;

	base *= base;
	window *= window;

	float amount = (1.5625f * base - 0.5625f) * window;

	colour += tap * amount;
	weight += amount;
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float2 position = uv * SourceSize.xy - 0.5f;
	float2 origin = floor(position);
	float2 fraction = position - origin;

	float3 b = Tap(origin, float2(0.0f, -1.0f));
	float3 c = Tap(origin, float2(1.0f, -1.0f));
	float3 e = Tap(origin, float2(-1.0f, 0.0f));
	float3 f = Tap(origin, float2(0.0f, 0.0f));
	float3 g = Tap(origin, float2(1.0f, 0.0f));
	float3 h = Tap(origin, float2(2.0f, 0.0f));
	float3 i = Tap(origin, float2(-1.0f, 1.0f));
	float3 j = Tap(origin, float2(0.0f, 1.0f));
	float3 k = Tap(origin, float2(1.0f, 1.0f));
	float3 l = Tap(origin, float2(2.0f, 1.0f));
	float3 n = Tap(origin, float2(0.0f, 2.0f));
	float3 o = Tap(origin, float2(1.0f, 2.0f));

	float2 dir = float2(0.0f, 0.0f);
	float len = 0.0f;

	Analyse(dir, len, (1.0f - fraction.x) * (1.0f - fraction.y), b.g, e.g, f.g, g.g, j.g);
	Analyse(dir, len, fraction.x * (1.0f - fraction.y), c.g, f.g, g.g, h.g, k.g);
	Analyse(dir, len, (1.0f - fraction.x) * fraction.y, f.g, i.g, j.g, k.g, n.g);
	Analyse(dir, len, fraction.x * fraction.y, g.g, j.g, k.g, l.g, o.g);

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

	float3 lowest = min(min(f, g), min(j, k));
	float3 highest = max(max(f, g), max(j, k));

	float3 colour = float3(0.0f, 0.0f, 0.0f);
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

	float3 pixel = min(highest, max(lowest, colour * rcp(weight)));

	return float4(pixel, tex2D(Scene, uv).a);
}
