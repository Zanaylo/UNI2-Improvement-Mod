#include "Game/BgShaderText.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kMarker = "TecBgNoSpeculer";
constexpr const char* kPani = "PS_TecBgPaniSpecular";
constexpr const char* kAssign = "out_color.rgb =";
constexpr const char* kBlack = "g_BgBlack.x ";
constexpr const char* kProduct = "(tex_color.rgb * In.color.rgb)";
constexpr const char* kGraded = "(tex_color.rgb * In.color.rgb * g_BgContrast.x)";

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
	return source.find(kMarker) != std::string::npos;
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
	float gameBlack, std::string& out, Result& result)
{
	result.black = 0;
	result.contrast = 0;
	out = source;

	if (!IsBackground(source))
		return false;

	const size_t pani = out.find(kPani);
	const size_t limit = pani == std::string::npos ? out.size() : pani;

	std::string head = out.substr(0, limit);
	result.black = Lifts(head);

	for (size_t at = head.find(kProduct); at != std::string::npos; at = head.find(kProduct, at))
	{
		head.replace(at, strlen(kProduct), kGraded);
		at += strlen(kGraded);
		++result.contrast;
	}

	if (result.black == 0)
		return false;

	out = head + out.substr(limit);

	char declared[192] = {};

	sprintf_s(declared, "float4 g_BgBlack : register(c%d) = float4(%.2ff, 0, 0, 0);\n"
		"float4 g_BgContrast : register(c%d) = float4(1.0f, 0, 0, 0);\n",
		blackRegister, gameBlack, contrastRegister);

	out.insert(0, declared);
	return true;
}
