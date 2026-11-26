sampler2D Frame : register(s0);

float4 TexelSize : register(c0);
float4 Quality : register(c1);

float Luma(float3 colour)
{
	return dot(colour, float3(0.299f, 0.587f, 0.114f));
}

float3 SampleFrame(float2 uv)
{
	return tex2Dlod(Frame, float4(uv, 0.0f, 0.0f)).rgb;
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float2 texel = TexelSize.xy;

	float3 centre = SampleFrame(uv);
	float lumaCentre = Luma(centre);

	float lumaDown = Luma(SampleFrame(uv + float2(0.0f, texel.y)));
	float lumaUp = Luma(SampleFrame(uv + float2(0.0f, -texel.y)));
	float lumaLeft = Luma(SampleFrame(uv + float2(-texel.x, 0.0f)));
	float lumaRight = Luma(SampleFrame(uv + float2(texel.x, 0.0f)));

	float lumaLow = min(lumaCentre, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
	float lumaHigh = max(lumaCentre, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
	float range = lumaHigh - lumaLow;

	if (range < max(Quality.y, lumaHigh * Quality.x))
		return float4(centre, 1.0f);

	float lumaDownLeft = Luma(SampleFrame(uv + float2(-texel.x, texel.y)));
	float lumaUpRight = Luma(SampleFrame(uv + float2(texel.x, -texel.y)));
	float lumaUpLeft = Luma(SampleFrame(uv + float2(-texel.x, -texel.y)));
	float lumaDownRight = Luma(SampleFrame(uv + float2(texel.x, texel.y)));

	float lumaDownUp = lumaDown + lumaUp;
	float lumaLeftRight = lumaLeft + lumaRight;

	float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
	float lumaDownCorners = lumaDownLeft + lumaDownRight;
	float lumaRightCorners = lumaDownRight + lumaUpRight;
	float lumaUpCorners = lumaUpRight + lumaUpLeft;

	float edgeHorizontal = abs(-2.0f * lumaLeft + lumaLeftCorners) +
		abs(-2.0f * lumaCentre + lumaDownUp) * 2.0f +
		abs(-2.0f * lumaRight + lumaRightCorners);

	float edgeVertical = abs(-2.0f * lumaUp + lumaUpCorners) +
		abs(-2.0f * lumaCentre + lumaLeftRight) * 2.0f +
		abs(-2.0f * lumaDown + lumaDownCorners);

	bool horizontal = edgeHorizontal >= edgeVertical;

	float lumaSideA = horizontal ? lumaDown : lumaLeft;
	float lumaSideB = horizontal ? lumaUp : lumaRight;

	float gradientA = lumaSideA - lumaCentre;
	float gradientB = lumaSideB - lumaCentre;

	bool steepestIsA = abs(gradientA) >= abs(gradientB);
	float gradientScaled = 0.25f * max(abs(gradientA), abs(gradientB));

	float stepLength = horizontal ? texel.y : texel.x;
	float lumaLocal = 0.0f;

	if (steepestIsA)
	{
		stepLength = -stepLength;
		lumaLocal = 0.5f * (lumaSideA + lumaCentre);
	}
	else
	{
		lumaLocal = 0.5f * (lumaSideB + lumaCentre);
	}

	float2 edgeUv = uv;

	if (horizontal)
		edgeUv.y += stepLength * 0.5f;
	else
		edgeUv.x += stepLength * 0.5f;

	float2 offset = horizontal ? float2(texel.x, 0.0f) : float2(0.0f, texel.y);

	float2 uvA = edgeUv - offset;
	float2 uvB = edgeUv + offset;

	float lumaEndA = Luma(SampleFrame(uvA)) - lumaLocal;
	float lumaEndB = Luma(SampleFrame(uvB)) - lumaLocal;

	bool reachedA = abs(lumaEndA) >= gradientScaled;
	bool reachedB = abs(lumaEndB) >= gradientScaled;

	if (!reachedA)
		uvA -= offset;

	if (!reachedB)
		uvB += offset;

	[loop]
	for (int i = 2; i < 12; ++i)
	{
		if (reachedA && reachedB)
			break;

		if (float(i) > Quality.w)
			break;

		if (!reachedA)
		{
			lumaEndA = Luma(SampleFrame(uvA)) - lumaLocal;
			reachedA = abs(lumaEndA) >= gradientScaled;
		}

		if (!reachedB)
		{
			lumaEndB = Luma(SampleFrame(uvB)) - lumaLocal;
			reachedB = abs(lumaEndB) >= gradientScaled;
		}

		float stride = (i < 5) ? 1.0f : ((i < 10) ? 2.0f : 4.0f);

		if (!reachedA)
			uvA -= offset * stride;

		if (!reachedB)
			uvB += offset * stride;
	}

	float distanceA = horizontal ? (uv.x - uvA.x) : (uv.y - uvA.y);
	float distanceB = horizontal ? (uvB.x - uv.x) : (uvB.y - uv.y);

	bool closerToA = distanceA < distanceB;
	float nearest = min(distanceA, distanceB);
	float thickness = distanceA + distanceB;

	float pixelOffset = -nearest / max(thickness, 0.0001f) + 0.5f;

	bool centreIsDarker = lumaCentre < lumaLocal;
	bool correctVariation = ((closerToA ? lumaEndA : lumaEndB) < 0.0f) != centreIsDarker;

	float finalOffset = correctVariation ? pixelOffset : 0.0f;

	float lumaAverage = (1.0f / 12.0f) * (2.0f * (lumaDownUp + lumaLeftRight) +
		lumaLeftCorners + lumaRightCorners);

	float subPixel = saturate(abs(lumaAverage - lumaCentre) / max(range, 0.0001f));
	float subPixelSmooth = (-2.0f * subPixel + 3.0f) * subPixel * subPixel;

	finalOffset = max(finalOffset, subPixelSmooth * subPixelSmooth * Quality.z);

	float2 finalUv = uv;

	if (horizontal)
		finalUv.y += finalOffset * stepLength;
	else
		finalUv.x += finalOffset * stepLength;

	return float4(SampleFrame(finalUv), 1.0f);
}
