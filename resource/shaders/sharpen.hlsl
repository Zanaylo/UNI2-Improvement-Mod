// Contrast adaptive sharpening over the finished frame.
//
// The game rasterises everything at 1280x720 and the composite blows that up to the window with a
// linear filter, which is where the softness comes from. This puts the edge contrast back without
// the halo an unsharp mask leaves, and it runs on the back buffer so it needs nothing from the
// engine. c0 is the texel size, c1.x the strength.

sampler2D Frame : register(s0);

float4 TexelSize : register(c0);
float4 Strength : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float2 step = TexelSize.xy;

	float3 e = tex2D(Frame, uv).rgb;
	float3 b = tex2D(Frame, uv + float2(0.0f, -step.y)).rgb;
	float3 h = tex2D(Frame, uv + float2(0.0f, step.y)).rgb;
	float3 d = tex2D(Frame, uv + float2(-step.x, 0.0f)).rgb;
	float3 f = tex2D(Frame, uv + float2(step.x, 0.0f)).rgb;
	float3 a = tex2D(Frame, uv + float2(-step.x, -step.y)).rgb;
	float3 c = tex2D(Frame, uv + float2(step.x, -step.y)).rgb;
	float3 g = tex2D(Frame, uv + float2(-step.x, step.y)).rgb;
	float3 i = tex2D(Frame, uv + float2(step.x, step.y)).rgb;

	float3 lowest = min(min(min(d, e), min(f, b)), h);
	lowest += min(lowest, min(min(a, c), min(g, i)));

	float3 highest = max(max(max(d, e), max(f, b)), h);
	highest += max(highest, max(max(a, c), max(g, i)));

	float3 amount = sqrt(saturate(min(lowest, 2.0f - highest) / max(highest, 0.0001f)));
	float3 weight = amount * (-1.0f / lerp(8.0f, 5.0f, saturate(Strength.x)));

	return float4(((b + d + f + h) * weight + e) / (1.0f + 4.0f * weight), 1.0f);
}
