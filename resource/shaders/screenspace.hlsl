float4 g_Map : register(c0);
float4 g_Present : register(c1);

struct Input
{
	float4 position : POSITION;
	float4 diffuse : COLOR0;
	float4 specular : COLOR1;
	float4 texcoord0 : TEXCOORD0;
	float4 texcoord1 : TEXCOORD1;
	float4 texcoord2 : TEXCOORD2;
	float4 texcoord3 : TEXCOORD3;
};

struct Output
{
	float4 position : POSITION;
	float4 diffuse : COLOR0;
	float4 specular : COLOR1;
	float4 texcoord0 : TEXCOORD0;
	float4 texcoord1 : TEXCOORD1;
	float4 texcoord2 : TEXCOORD2;
	float4 texcoord3 : TEXCOORD3;
};

Output main(Input input)
{
	Output output;

	float w = 1.0 / input.position.w;
	output.position = float4((input.position.xy * g_Map.xy + g_Map.zw) * w, input.position.z * w, w);
	output.diffuse = lerp(float4(1.0, 1.0, 1.0, 1.0), input.diffuse, g_Present.x);
	output.specular = input.specular * g_Present.y;
	output.texcoord0 = input.texcoord0;
	output.texcoord1 = input.texcoord1;
	output.texcoord2 = input.texcoord2;
	output.texcoord3 = input.texcoord3;

	return output;
}
