#include "Game/Stages/BgShaderText.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kMarker = "TecBgNoSpeculer";
constexpr const char* kPani = "PS_TecBgPaniSpecular";
constexpr const char* kAssign = "out_color.rgb =";
constexpr const char* kBlack = "g_BgBlack.rgb ";
constexpr const char* kProduct = "(tex_color.rgb * In.color.rgb)";
constexpr const char* kGraded =
	"(tex_color.rgb * In.color.rgb * g_BgContrast.rgb * BgLamp(In.texuv))";
constexpr const char* kFetch = "tex2D( TexSample_Base, In.texuv )";
constexpr const char* kAlpha = "out_color.a = tex_color.a;";
constexpr const char* kFaded = "out_color.a = tex_color.a * BgFade(In.color.a);";
constexpr const char* kFlowed = "tex2D( TexSample_Base, BgFlow(In.texuv) )";
constexpr float kFlowMark = 64.0f;
constexpr float kFlowStride = 128.0f;
constexpr float kFlowSlots = 8.0f;
constexpr const char* kFbxMarker = "Diffuse_PS_Easy";
constexpr const char* kFbxFetch = "tex2D( diffuseSampler, input.UV )";
constexpr const char* kFbxFlowed = "tex2D( diffuseSampler, BgFlow(input.UV.xy) )";
constexpr const char* kFbxModulate = "OutColor *= input.Color;";
constexpr const char* kFbxGraded =
	"OutColor *= input.Color; OutColor.rgb *= g_BgContrast.rgb * g_BgContrast.w * BgLamp(input.UV.xy);";

size_t SkipSpace(const std::string& text, size_t at)
{
	while (at < text.size() && (text[at] == ' ' || text[at] == '\t'))
		++at;

	return at;
}

int Lifts(std::string& head)
{
	const size_t key = strlen(kAssign);
	int changed = 0;

	for (size_t at = head.find(kAssign); at != std::string::npos; at = head.find(kAssign, at))
	{
		const size_t value = SkipSpace(head, at + key);
		size_t stop = value;

		while (stop < head.size() && ((head[stop] >= '0' && head[stop] <= '9')
			|| head[stop] == '.'))
		{
			++stop;
		}

		if (stop == value)
		{
			at += key;
			continue;
		}

		size_t plus = stop;

		if (plus < head.size() && (head[plus] == 'f' || head[plus] == 'F'))
			++plus;

		plus = SkipSpace(head, plus);

		if (plus >= head.size() || head[plus] != '+')
		{
			at += key;
			continue;
		}

		head.replace(value, plus - value, kBlack);
		at = value + strlen(kBlack);
		++changed;
	}

	return changed;
}

}

bool BgShaderText::IsBackground(const std::string& source)
{
	return source.find(kMarker) != std::string::npos
		|| source.find(kFbxMarker) != std::string::npos;
}

std::string BgShaderText::Assignment(const std::string& source)
{
	const size_t at = source.find(kAssign);

	if (at == std::string::npos)
		return std::string();

	const size_t stop = source.find(';', at);

	return source.substr(at, stop == std::string::npos ? 80 : stop - at + 1);
}

bool BgShaderText::Rewrite(const std::string& source, int blackRegister, int contrastRegister,
	int flowRegister, int lampRegister, float gameBlack, std::string& out, Result& result)
{
	result.black = 0;
	result.contrast = 0;
	result.flow = 0;
	result.fade = 0;
	out = source;

	if (!IsBackground(source))
		return false;

	const bool fbx = out.find(kFbxMarker) != std::string::npos;
	const size_t pani = out.find(kPani);
	const size_t limit = fbx || pani == std::string::npos ? out.size() : pani;

	std::string head = out.substr(0, limit);
	result.black = fbx ? 0 : Lifts(head);

	const char* const product = fbx ? kFbxModulate : kProduct;
	const char* const graded = fbx ? kFbxGraded : kGraded;
	const char* const fetch = fbx ? kFbxFetch : kFetch;
	const char* const flowed = fbx ? kFbxFlowed : kFlowed;

	for (size_t at = head.find(product); at != std::string::npos; at = head.find(product, at))
	{
		head.replace(at, strlen(product), graded);
		at += strlen(graded);
		++result.contrast;
	}

	for (size_t at = head.find(fetch); at != std::string::npos; at = head.find(fetch, at))
	{
		head.replace(at, strlen(fetch), flowed);
		at += strlen(flowed);
		++result.flow;
	}

	for (size_t at = head.find(kAlpha); at != std::string::npos; at = head.find(kAlpha, at))
	{
		head.replace(at, strlen(kAlpha), kFaded);
		at += strlen(kFaded);
		++result.fade;
	}

	if (fbx ? result.contrast == 0 : result.black == 0)
		return false;

	out = head + out.substr(limit);

	char declared[2560] = {};

	sprintf_s(declared, "float4 g_BgLamp0 : register(c%d) = float4(1, 1, 1, 1);\n"
		"float4 g_BgLamp1 : register(c%d) = float4(1, 1, 1, 1);\n"
		"float BgLamp(float2 uv)\n"
		"{\n"
		"\tif (uv.y < %.1ff) return 1.0f;\n"
		"\tfloat slot = floor((uv.y - %.1ff) / %.1ff);\n"
		"\tfloat level = g_BgLamp0.x;\n"
		"\tlevel = slot > 0.5f ? g_BgLamp0.y : level;\n"
		"\tlevel = slot > 1.5f ? g_BgLamp0.z : level;\n"
		"\tlevel = slot > 2.5f ? g_BgLamp0.w : level;\n"
		"\tlevel = slot > 3.5f ? g_BgLamp1.x : level;\n"
		"\tlevel = slot > 4.5f ? g_BgLamp1.y : level;\n"
		"\tlevel = slot > 5.5f ? g_BgLamp1.z : level;\n"
		"\tlevel = slot > 6.5f ? g_BgLamp1.w : level;\n"
		"\treturn level;\n"
		"}\n"
		"float4 g_BgBlack : register(c%d) = float4(%.2ff, %.2ff, %.2ff, 0);\n"
		"float4 g_BgContrast : register(c%d) = float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
		"float4 g_BgFlow0 : register(c%d) = float4(0, 0, 0, 0);\n"
		"float4 g_BgFlow1 : register(c%d) = float4(0, 0, 0, 0);\n"
		"float BgFade(float alpha)\n"
		"{\n"
		"\treturn 1.0f - g_BgBlack.w + g_BgBlack.w * alpha;\n"
		"}\n"
		"float2 BgFlow(float2 uv)\n"
		"{\n"
		"\tif (uv.x < %.1ff) return uv;\n"
		"\tfloat slot = floor((uv.x - %.1ff) / %.1ff);\n"
		"\tfloat across = slot >= %.1ff ? 1.0f : 0.0f;\n"
		"\tfloat which = slot - across * %.1ff;\n"
		"\tfloat speed = g_BgFlow0.x;\n"
		"\tspeed = which > 0.5f ? g_BgFlow0.y : speed;\n"
		"\tspeed = which > 1.5f ? g_BgFlow0.z : speed;\n"
		"\tspeed = which > 2.5f ? g_BgFlow0.w : speed;\n"
		"\tspeed = which > 3.5f ? g_BgFlow1.x : speed;\n"
		"\tspeed = which > 4.5f ? g_BgFlow1.y : speed;\n"
		"\tspeed = which > 5.5f ? g_BgFlow1.z : speed;\n"
		"\tspeed = which > 6.5f ? g_BgFlow1.w : speed;\n"
		"\treturn float2(uv.x + across * speed, uv.y + (1.0f - across) * speed);\n"
		"}\n",
		lampRegister, lampRegister + 1, kFlowMark, kFlowMark, kFlowStride,
		blackRegister, gameBlack, gameBlack, gameBlack, contrastRegister,
		flowRegister, flowRegister + 1,
		kFlowMark, kFlowMark, kFlowStride, kFlowSlots, kFlowSlots);

	out.insert(0, declared);
	return true;
}
