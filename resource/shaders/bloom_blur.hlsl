sampler2D Frame : register(s0);

float4 Step : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float2 offset = Step.xy;

	float3 colour = tex2D(Frame, uv).rgb * 0.227027f;

	colour += (tex2D(Frame, uv + offset * 1.3846154f).rgb +
		tex2D(Frame, uv - offset * 1.3846154f).rgb) * 0.3162162f;

	colour += (tex2D(Frame, uv + offset * 3.2307692f).rgb +
		tex2D(Frame, uv - offset * 3.2307692f).rgb) * 0.0702703f;

	return float4(colour, 1.0f);
}
