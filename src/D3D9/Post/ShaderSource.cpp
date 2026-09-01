#include "D3D9/Post/ShaderSource.h"

#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Extension
{
	const char* text;
	ShaderSource::Format format;
};

constexpr Extension kExtensions[] = {
	{ ".hlsl", ShaderSource::Format_Hlsl },
	{ ".ps", ShaderSource::Format_Hlsl },
	{ ".fx", ShaderSource::Format_ReShade },
	{ ".slang", ShaderSource::Format_Glsl },
	{ ".glsl", ShaderSource::Format_Glsl },
	{ ".frag", ShaderSource::Format_Glsl },
	{ ".fsh", ShaderSource::Format_Glsl },
};

constexpr int kExtensionCount = static_cast<int>(sizeof(kExtensions) / sizeof(kExtensions[0]));

constexpr const char* kBindings =
	"sampler2D Frame : register(s0);\n"
	"float4 FrameSize : register(c0);\n"
	"float4 FrameTime : register(c1);\n";

constexpr const char* kReShadePrologue =
	"#define __RESHADE__ 50000\n"
	"#define __RESHADE_PERFORMANCE_MODE__ 1\n"
	"#define __RENDERER__ 0x9000\n"
	"#define BUFFER_WIDTH FrameSize.z\n"
	"#define BUFFER_HEIGHT FrameSize.w\n"
	"#define BUFFER_RCP_WIDTH FrameSize.x\n"
	"#define BUFFER_RCP_HEIGHT FrameSize.y\n"
	"#define BUFFER_PIXEL_SIZE FrameSize.xy\n"
	"#define BUFFER_SCREEN_SIZE FrameSize.zw\n"
	"#define BUFFER_ASPECT_RATIO (FrameSize.z / FrameSize.w)\n"
	"#define BUFFER_COLOR_DEPTH 8\n"
	"#define BUFFER_COLOR_BIT_DEPTH 8\n"
	"float ReShadeLinearDepth(float2 uv) { return 1.0; }\n"
	"float4 ReShadeFetch(sampler2D source, int2 pixel)\n"
	"{\n"
	"\treturn tex2Dlod(source, float4((pixel + 0.5) * FrameSize.xy, 0.0, 0.0));\n"
	"}\n"
	"#define tex2Dfetch(s, p) ReShadeFetch(s, p)\n"
	"#define tex2Doffset(s, c, o) tex2D(s, (c) + (o) * FrameSize.xy)\n";

constexpr const char* kGlslPrologue =
	"#define FRAGMENT 1\n"
	"#define __VERSION__ 450\n";

struct Named
{
	const char* name;
	const char* value;
};

constexpr Named kLibretroNames[] = {
	{ "SourceSize", "float4(FrameSize.zw, FrameSize.xy)" },
	{ "OriginalSize", "float4(FrameSize.zw, FrameSize.xy)" },
	{ "OutputSize", "float4(FrameSize.zw, FrameSize.xy)" },
	{ "FinalViewportSize", "float4(FrameSize.zw, FrameSize.xy)" },
	{ "TextureSize", "FrameSize.zw" },
	{ "InputSize", "FrameSize.zw" },
	{ "OutputSize0", "float4(FrameSize.zw, FrameSize.xy)" },
	{ "FrameCount", "FrameTime.y" },
	{ "FrameDirection", "1.0" },
	{ "MVP", "float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)" },
	{ "MVPMatrix", "float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)" },
};

constexpr Named kShaderToyNames[] = {
	{ "iResolution", "float3(FrameSize.zw, 1.0)" },
	{ "iTime", "FrameTime.x" },
	{ "iGlobalTime", "FrameTime.x" },
	{ "iTimeDelta", "(1.0 / 60.0)" },
	{ "iFrameRate", "60.0" },
	{ "iFrame", "((int)FrameTime.y)" },
	{ "iMouse", "float4(0.0, 0.0, 0.0, 0.0)" },
	{ "iDate", "float4(0.0, 0.0, 0.0, 0.0)" },
	{ "iChannel0", "Frame" },
	{ "iChannel1", "Frame" },
	{ "iChannel2", "Frame" },
	{ "iChannel3", "Frame" },
	{ "iChannelResolution", "float3(FrameSize.zw, 1.0)" },
	{ "iChannelTime", "FrameTime.x" },
};

struct Word
{
	const char* from;
	const char* to;
};

constexpr Word kGlslWords[] = {
	{ "vec2", "float2" }, { "vec3", "float3" }, { "vec4", "float4" },
	{ "ivec2", "int2" }, { "ivec3", "int3" }, { "ivec4", "int4" },
	{ "uvec2", "uint2" }, { "uvec3", "uint3" }, { "uvec4", "uint4" },
	{ "bvec2", "bool2" }, { "bvec3", "bool3" }, { "bvec4", "bool4" },
	{ "mat2", "float2x2" }, { "mat3", "float3x3" }, { "mat4", "float4x4" },
	{ "mat2x2", "float2x2" }, { "mat3x3", "float3x3" }, { "mat4x4", "float4x4" },
	{ "mix", "lerp" }, { "fract", "frac" }, { "mod", "fmod" },
	{ "dFdx", "ddx" }, { "dFdy", "ddy" }, { "inversesqrt", "rsqrt" },
	{ "texture2DLod", "tex2Dlod" }, { "texture2DProj", "tex2Dproj" },
	{ "textureLod", "tex2Dlod" }, { "texture2D", "tex2D" }, { "texture", "tex2D" },
	{ "highp", "" }, { "mediump", "" }, { "lowp", "" },
};

struct Parameter
{
	std::string type;
	bool isOut;
};

typedef std::vector<std::pair<std::string, std::string> > DefineList;

bool IsWordChar(char character)
{
	return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
}

bool IsSpace(char character)
{
	return std::isspace(static_cast<unsigned char>(character)) != 0;
}

bool WordAt(const std::string& text, size_t index, const char* word)
{
	const size_t length = strlen(word);

	if (text.compare(index, length, word) != 0)
		return false;

	if (index > 0 && IsWordChar(text[index - 1]))
		return false;

	return index + length >= text.size() || !IsWordChar(text[index + length]);
}

size_t FindWord(const std::string& text, const char* word, size_t from)
{
	for (size_t index = text.find(word, from); index != std::string::npos;
		index = text.find(word, index + 1))
	{
		if (WordAt(text, index, word))
			return index;
	}

	return std::string::npos;
}

void ReplaceAll(std::string& text, const char* from, const char* to)
{
	const size_t length = strlen(from);
	const std::string replacement = to;
	size_t index = 0;

	while ((index = text.find(from, index)) != std::string::npos)
	{
		text.replace(index, length, replacement);
		index += replacement.size();
	}
}

void ReplaceWord(std::string& text, const char* from, const char* to)
{
	const size_t length = strlen(from);
	const std::string replacement = to;
	size_t index = 0;

	while ((index = FindWord(text, from, index)) != std::string::npos)
	{
		text.replace(index, length, replacement);
		index += replacement.size();
	}
}

size_t SkipSpace(const std::string& text, size_t index)
{
	while (index < text.size() && IsSpace(text[index]))
		++index;

	return index;
}

std::string ReadWord(const std::string& text, size_t& index)
{
	index = SkipSpace(text, index);
	const size_t start = index;

	while (index < text.size() && IsWordChar(text[index]))
		++index;

	return text.substr(start, index - start);
}

size_t MatchBracket(const std::string& text, size_t open, char opener, char closer)
{
	int depth = 0;

	for (size_t index = open; index < text.size(); ++index)
	{
		if (text[index] == opener)
		{
			++depth;
			continue;
		}

		if (text[index] == closer && --depth == 0)
			return index;
	}

	return std::string::npos;
}

std::string Trimmed(const std::string& text)
{
	size_t first = 0;

	while (first < text.size() && IsSpace(text[first]))
		++first;

	size_t last = text.size();

	while (last > first && IsSpace(text[last - 1]))
		--last;

	return text.substr(first, last - first);
}

std::string SingleLine(const std::string& text)
{
	std::string out = text;

	for (char& character : out)
	{
		if (character == '\n' || character == '\r')
			character = ' ';
	}

	return out;
}

std::string StripComments(const std::string& text)
{
	std::string out;
	out.reserve(text.size());

	size_t index = 0;

	while (index < text.size())
	{
		const char character = text[index];

		if (character == '"')
		{
			const size_t end = text.find('"', index + 1);
			const size_t stop = end == std::string::npos ? text.size() : end + 1;

			out.append(text, index, stop - index);
			index = stop;
			continue;
		}

		if (character == '/' && index + 1 < text.size() && text[index + 1] == '/')
		{
			const size_t end = text.find('\n', index);

			if (end == std::string::npos)
				break;

			index = end;
			continue;
		}

		if (character == '/' && index + 1 < text.size() && text[index + 1] == '*')
		{
			const size_t end = text.find("*/", index + 2);
			out.push_back(' ');

			if (end == std::string::npos)
				break;

			index = end + 2;
			continue;
		}

		out.push_back(character);
		++index;
	}

	return out;
}

bool HasDefine(const DefineList& defines, const std::string& name)
{
	for (const std::pair<std::string, std::string>& define : defines)
	{
		if (define.first == name)
			return true;
	}

	return false;
}

void AddDefine(DefineList& defines, const std::string& name, const std::string& value)
{
	if (name.empty() || HasDefine(defines, name))
		return;

	defines.push_back(std::make_pair(name, value));
}

std::string DefineText(const DefineList& defines)
{
	std::string text;

	for (const std::pair<std::string, std::string>& define : defines)
		text += "#define " + define.first + " " + define.second + "\n";

	return text;
}

bool IsQualifier(const std::string& word)
{
	static const char* const kWords[] = { "in", "out", "inout", "uniform", "const", "precise",
		"nointerpolation", "linear", "centroid", "noperspective", "sample", "highp", "mediump",
		"lowp", "static", "COMPAT_PRECISION" };

	for (const char* const candidate : kWords)
	{
		if (word == candidate)
			return true;
	}

	return false;
}

void AppendParameter(const std::string& text, std::vector<Parameter>& out)
{
	const size_t semantic = text.find(':');
	const std::string body = Trimmed(text.substr(0, semantic));

	if (body.empty())
		return;

	Parameter parameter = {};
	size_t index = 0;

	while (index < body.size())
	{
		const size_t before = index;
		const std::string word = ReadWord(body, index);

		if (word.empty())
		{
			index = before + 1;
			continue;
		}

		if (word == "out" || word == "inout")
		{
			parameter.isOut = true;
			continue;
		}

		if (IsQualifier(word))
			continue;

		if (parameter.type.empty())
		{
			parameter.type = word;
			continue;
		}

		break;
	}

	if (parameter.type.empty())
		return;

	out.push_back(parameter);
}

void SplitParameters(const std::string& list, std::vector<Parameter>& out)
{
	int depth = 0;
	std::string current;

	for (size_t index = 0; index <= list.size(); ++index)
	{
		const char character = index < list.size() ? list[index] : ',';

		if (character == '(' || character == '<')
			++depth;

		if (character == ')' || character == '>')
			--depth;

		if (character == ',' && depth <= 0)
		{
			AppendParameter(current, out);
			current.clear();
			continue;
		}

		current.push_back(character);
	}
}

bool FindFunction(const std::string& text, const std::string& name, std::string& outReturnType,
	std::vector<Parameter>& outParameters)
{
	size_t index = 0;

	while ((index = FindWord(text, name.c_str(), index)) != std::string::npos)
	{
		const size_t open = SkipSpace(text, index + name.size());

		if (open >= text.size() || text[open] != '(')
		{
			index += name.size();
			continue;
		}

		const size_t close = MatchBracket(text, open, '(', ')');

		if (close == std::string::npos)
			return false;

		const size_t after = SkipSpace(text, close + 1);

		if (after >= text.size() || (text[after] != '{' && text[after] != ':'))
		{
			index = close;
			continue;
		}

		size_t before = index;

		while (before > 0 && IsSpace(text[before - 1]))
			--before;

		const size_t typeEnd = before;

		while (before > 0 && IsWordChar(text[before - 1]))
			--before;

		outReturnType = text.substr(before, typeEnd - before);
		outParameters.clear();
		SplitParameters(text.substr(open + 1, close - open - 1), outParameters);
		return true;
	}

	return false;
}

std::string ArgumentFor(const Parameter& parameter)
{
	if (parameter.isOut)
		return "outColour";

	if (parameter.type == "float4")
		return "float4(uv * FrameSize.zw, 0.0, 1.0)";

	if (parameter.type == "float2")
		return "uv";

	return "(" + parameter.type + ")0";
}

std::string BuildEntry(const std::string& name, const std::string& returnType,
	const std::vector<Parameter>& parameters)
{
	std::string arguments;

	for (size_t index = 0; index < parameters.size(); ++index)
	{
		if (index > 0)
			arguments += ", ";

		arguments += ArgumentFor(parameters[index]);
	}

	std::string body = "\nfloat4 main(float2 uv : TEXCOORD0) : COLOR0\n{\n";
	body += "\tfloat4 outColour = float4(0.0, 0.0, 0.0, 1.0);\n";

	if (returnType == "void")
		body += "\t" + name + "(" + arguments + ");\n";
	else
		body += "\toutColour = " + name + "(" + arguments + ");\n";

	body += "\treturn outColour;\n}\n";
	return body;
}

void StripSemantics(std::string& text)
{
	size_t index = 0;

	while ((index = text.find(':', index)) != std::string::npos)
	{
		if (index + 1 < text.size() && text[index + 1] == ':')
		{
			index += 2;
			continue;
		}

		size_t cursor = index + 1;
		const std::string word = ReadWord(text, cursor);

		if (_strnicmp(word.c_str(), "SV_", 3) != 0 && _stricmp(word.c_str(), "VPOS") != 0)
		{
			++index;
			continue;
		}

		text.erase(index, cursor - index);
	}
}

void RemoveIncludes(std::string& text)
{
	size_t index = 0;

	while ((index = text.find("#include", index)) != std::string::npos)
	{
		size_t end = text.find('\n', index);

		if (end == std::string::npos)
			end = text.size();

		text.erase(index, end - index);
	}
}

std::string AnnotationSource(const std::string& annotation)
{
	const size_t index = FindWord(annotation, "source", 0);

	if (index == std::string::npos)
		return "";

	const size_t quote = annotation.find('"', index);

	if (quote == std::string::npos)
		return "";

	const size_t end = annotation.find('"', quote + 1);

	if (end == std::string::npos)
		return "";

	return annotation.substr(quote + 1, end - quote - 1);
}

std::string SourceExpression(const std::string& source, const std::string& type)
{
	if (source == "timer")
		return "FrameTime.x * 1000.0";

	if (source == "frametime")
		return "1000.0 / 60.0";

	if (source == "framecount")
		return "(int)FrameTime.y";

	return "(" + type + ")0";
}

void ConvertReShadeUniforms(std::string& text)
{
	size_t index = 0;

	while ((index = FindWord(text, "uniform", index)) != std::string::npos)
	{
		size_t cursor = index + 7;
		const std::string type = ReadWord(text, cursor);
		const std::string name = ReadWord(text, cursor);

		if (type.empty() || name.empty())
		{
			index += 7;
			continue;
		}

		std::string annotation;
		cursor = SkipSpace(text, cursor);

		if (cursor < text.size() && text[cursor] == '<')
		{
			const size_t close = MatchBracket(text, cursor, '<', '>');

			if (close == std::string::npos)
				return;

			annotation = text.substr(cursor + 1, close - cursor - 1);
			cursor = close + 1;
		}

		cursor = SkipSpace(text, cursor);
		std::string value;

		if (cursor < text.size() && text[cursor] == '=')
		{
			const size_t semicolon = text.find(';', cursor);

			if (semicolon == std::string::npos)
				return;

			value = Trimmed(SingleLine(text.substr(cursor + 1, semicolon - cursor - 1)));
			cursor = semicolon;
		}

		const size_t end = text.find(';', cursor);

		if (end == std::string::npos)
			return;

		const std::string source = AnnotationSource(annotation);

		if (!source.empty())
			value = SourceExpression(source, type);
		else if (value.empty())
			value = "(" + type + ")0";

		const std::string replacement = "\n#define " + name + " (" + value + ")\n";
		text.replace(index, end + 1 - index, replacement);
		index += replacement.size();
	}
}

bool ReadStateDeclaration(const std::string& text, size_t start, size_t nameEnd, size_t& outEnd)
{
	size_t cursor = SkipSpace(text, nameEnd);

	if (cursor < text.size() && text[cursor] == '<')
	{
		const size_t close = MatchBracket(text, cursor, '<', '>');

		if (close == std::string::npos)
			return false;

		cursor = SkipSpace(text, close + 1);
	}

	if (cursor >= text.size() || text[cursor] != '{')
		return false;

	const size_t close = MatchBracket(text, cursor, '{', '}');

	if (close == std::string::npos)
		return false;

	size_t end = SkipSpace(text, close + 1);

	if (end < text.size() && text[end] == ';')
		++end;

	outEnd = end;
	return start < end;
}

void ConvertReShadeObjects(std::string& text, const char* const* keywords, int count,
	const char* define)
{
	for (int keyword = 0; keyword < count; ++keyword)
	{
		const size_t length = strlen(keywords[keyword]);
		size_t index = 0;

		while ((index = FindWord(text, keywords[keyword], index)) != std::string::npos)
		{
			size_t cursor = index + length;
			const std::string name = ReadWord(text, cursor);
			size_t end = 0;

			if (name.empty() || !ReadStateDeclaration(text, index, cursor, end))
			{
				index += length;
				continue;
			}

			const std::string replacement =
				define == nullptr ? std::string() : "\n#define " + name + " " + define + "\n";

			text.replace(index, end - index, replacement);
			index += replacement.size();
		}
	}
}

std::string PixelShaderName(const std::string& block)
{
	const size_t index = FindWord(block, "PixelShader", 0);

	if (index == std::string::npos)
		return "";

	const size_t equals = block.find('=', index);

	if (equals == std::string::npos)
		return "";

	size_t cursor = equals + 1;
	return ReadWord(block, cursor);
}

void ExtractTechnique(std::string& text, std::string& outEntry)
{
	size_t index = 0;

	while ((index = FindWord(text, "technique", index)) != std::string::npos)
	{
		size_t cursor = index + 9;
		ReadWord(text, cursor);
		cursor = SkipSpace(text, cursor);

		if (cursor < text.size() && text[cursor] == '<')
		{
			const size_t close = MatchBracket(text, cursor, '<', '>');

			if (close == std::string::npos)
				return;

			cursor = SkipSpace(text, close + 1);
		}

		if (cursor >= text.size() || text[cursor] != '{')
		{
			index += 9;
			continue;
		}

		const size_t close = MatchBracket(text, cursor, '{', '}');

		if (close == std::string::npos)
			return;

		if (outEntry.empty())
			outEntry = PixelShaderName(text.substr(cursor, close - cursor + 1));

		size_t end = close + 1;

		if (end < text.size() && text[end] == ';')
			++end;

		text.erase(index, end - index);
	}
}

bool TranslateReShade(const std::string& source, std::string& outHlsl, std::string& outNote)
{
	static const char* const kTextures[] = { "texture", "texture2D", "Texture2D" };
	static const char* const kSamplers[] = { "sampler", "sampler2D", "sampler1D", "sampler3D",
		"SamplerState" };

	std::string text = StripComments(source);

	RemoveIncludes(text);
	ConvertReShadeUniforms(text);
	ConvertReShadeObjects(text, kTextures, 3, nullptr);
	ConvertReShadeObjects(text, kSamplers, 5, "Frame");

	std::string entry;
	ExtractTechnique(text, entry);

	ReplaceAll(text, "ReShade::BackBufferTex", "Frame");
	ReplaceAll(text, "ReShade::BackBuffer", "Frame");
	ReplaceAll(text, "ReShade::DepthBuffer", "Frame");
	ReplaceAll(text, "ReShade::PixelSize", "FrameSize.xy");
	ReplaceAll(text, "ReShade::ScreenSize", "FrameSize.zw");
	ReplaceAll(text, "ReShade::AspectRatio", "(FrameSize.z / FrameSize.w)");
	ReplaceAll(text, "ReShade::GetLinearizedDepth", "ReShadeLinearDepth");
	ReplaceAll(text, "ReShade::", "");

	StripSemantics(text);

	if (entry.empty())
	{
		outNote = "no technique naming a PixelShader, so there is no pass to run";
		return false;
	}

	std::string returnType;
	std::vector<Parameter> parameters;

	if (!FindFunction(text, entry, returnType, parameters))
	{
		outNote = entry + " is named by the technique but not defined in this file";
		return false;
	}

	outHlsl = std::string(kBindings) + kReShadePrologue + "\n" + text +
		BuildEntry(entry, returnType, parameters);

	outNote = "ReShade .fx, the first technique's pass through " + entry;
	return true;
}

void SelectFragmentStage(std::string& text)
{
	const size_t first = text.find("#pragma stage");

	if (first == std::string::npos)
		return;

	const std::string shared = text.substr(0, first);
	size_t fragment = text.find("#pragma stage fragment");

	if (fragment == std::string::npos)
	{
		text = shared;
		return;
	}

	fragment = text.find('\n', fragment);

	if (fragment == std::string::npos)
	{
		text = shared;
		return;
	}

	size_t end = text.find("#pragma stage", fragment);

	if (end == std::string::npos)
		end = text.size();

	text = shared + text.substr(fragment, end - fragment);
}

void RemoveLayoutQualifiers(std::string& text)
{
	size_t index = 0;

	while ((index = FindWord(text, "layout", index)) != std::string::npos)
	{
		const size_t open = SkipSpace(text, index + 6);

		if (open >= text.size() || text[open] != '(')
		{
			index += 6;
			continue;
		}

		const size_t close = MatchBracket(text, open, '(', ')');

		if (close == std::string::npos)
			return;

		text.erase(index, close + 1 - index);
	}
}

void SplitTokens(const std::string& line, std::vector<std::string>& out)
{
	size_t index = 0;

	while (index < line.size())
	{
		if (IsSpace(line[index]))
		{
			++index;
			continue;
		}

		if (line[index] == '"')
		{
			const size_t end = line.find('"', index + 1);
			const size_t stop = end == std::string::npos ? line.size() : end + 1;

			out.push_back(line.substr(index, stop - index));
			index = stop;
			continue;
		}

		const size_t start = index;

		while (index < line.size() && !IsSpace(line[index]))
			++index;

		out.push_back(line.substr(start, index - start));
	}
}

bool IsDroppedDirective(const std::string& line)
{
	static const char* const kPrefixes[] = { "#version", "#extension", "#pragma", "precision " };

	for (const char* const prefix : kPrefixes)
	{
		if (line.compare(0, strlen(prefix), prefix) == 0)
			return true;
	}

	return false;
}

std::string FilterGlslLines(const std::string& text, DefineList& defines)
{
	std::string out;
	size_t index = 0;

	while (index <= text.size())
	{
		size_t end = text.find('\n', index);

		if (end == std::string::npos)
			end = text.size();

		const std::string line = text.substr(index, end - index);
		const std::string trimmed = Trimmed(line);

		if (trimmed.compare(0, 18, "#pragma parameter ") == 0)
		{
			std::vector<std::string> tokens;
			SplitTokens(trimmed, tokens);

			if (tokens.size() >= 5)
				AddDefine(defines, tokens[2], "(" + tokens[4] + ")");
		}
		else if (!IsDroppedDirective(trimmed))
		{
			out += line;
		}

		out += '\n';

		if (end >= text.size())
			break;

		index = end + 1;
	}

	return out;
}

std::string ReadType(const std::string& text, size_t& index)
{
	while (index < text.size())
	{
		const size_t before = index;
		const std::string word = ReadWord(text, index);

		if (word.empty())
		{
			index = before;
			return "";
		}

		if (!IsQualifier(word))
			return word;
	}

	return "";
}

bool IsSamplerType(const std::string& type)
{
	return type.compare(0, 7, "sampler") == 0 || type.compare(0, 8, "isampler") == 0;
}

bool IsVaryingKeyword(const std::string& word)
{
	return word == "in" || word == "varying" || word == "attribute" || word == "COMPAT_VARYING";
}

bool IsLibretroName(const std::string& name)
{
	for (const Named& known : kLibretroNames)
	{
		if (name == known.name)
			return true;
	}

	return false;
}

bool Mentions(const std::string& name, const char* word)
{
	std::string lowered = name;

	for (char& character : lowered)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

	return lowered.find(word) != std::string::npos;
}

std::string GuessedUniform(const std::string& type, const std::string& name)
{
	const bool resolution = Mentions(name, "resolution") || Mentions(name, "size");

	if (resolution && (type == "vec2" || type == "float2"))
		return "FrameSize.zw";

	if (resolution && (type == "vec3" || type == "float3"))
		return "float3(FrameSize.zw, 1.0)";

	if (resolution && (type == "vec4" || type == "float4"))
		return "float4(FrameSize.zw, FrameSize.xy)";

	if (type == "float" && Mentions(name, "time"))
		return "FrameTime.x";

	if ((type == "int" || type == "uint") && Mentions(name, "frame"))
		return "((int)FrameTime.y)";

	return "((" + type + ")0)";
}

size_t SkipDirectiveLine(const std::string& text, size_t index)
{
	const size_t end = text.find('\n', index);

	return end == std::string::npos ? text.size() : end + 1;
}

bool StatementEnd(const std::string& text, size_t start, size_t& outEnd)
{
	for (size_t index = start; index < text.size(); ++index)
	{
		if (text[index] == ';')
		{
			outEnd = index + 1;
			return true;
		}

		if (text[index] == '{' || text[index] == '#')
			return false;
	}

	return false;
}

bool ConvertUniformBlock(std::string& text, size_t start, size_t nameEnd,
	std::vector<std::string>& instances)
{
	const size_t open = SkipSpace(text, nameEnd);

	if (open >= text.size() || text[open] != '{')
		return false;

	const size_t close = MatchBracket(text, open, '{', '}');

	if (close == std::string::npos)
		return false;

	size_t cursor = close + 1;
	const std::string instance = ReadWord(text, cursor);

	if (!instance.empty())
		instances.push_back(instance);

	cursor = SkipSpace(text, cursor);

	if (cursor < text.size() && text[cursor] == ';')
		++cursor;

	text.erase(start, cursor - start);
	return true;
}

bool ConvertUniformDeclaration(std::string& text, size_t start, size_t keywordEnd,
	DefineList& defines, std::vector<std::string>& instances)
{
	size_t cursor = keywordEnd;
	const std::string first = ReadType(text, cursor);

	if (first.empty())
		return false;

	if (ConvertUniformBlock(text, start, cursor, instances))
		return true;

	const std::string name = ReadWord(text, cursor);
	size_t end = 0;

	if (name.empty() || !StatementEnd(text, cursor, end))
		return false;

	if (IsSamplerType(first))
		AddDefine(defines, name, "Frame");
	else if (!IsLibretroName(name) && !HasDefine(defines, name))
		AddDefine(defines, name, GuessedUniform(first, name));

	text.erase(start, end - start);
	return true;
}

bool ConvertVaryingDeclaration(std::string& text, size_t start, size_t keywordEnd,
	DefineList& defines, bool isOutput)
{
	size_t cursor = keywordEnd;
	const std::string type = ReadType(text, cursor);
	const std::string name = ReadWord(text, cursor);
	size_t end = 0;

	if (type.empty() || name.empty() || !StatementEnd(text, cursor, end))
		return false;

	if (isOutput)
		AddDefine(defines, name, "FragColourOut");
	else if (type == "vec2" || type == "float2")
		AddDefine(defines, name, "FragCoordUv");
	else
		AddDefine(defines, name, "float4(FragCoordUv, 0.0, 0.0)");

	text.erase(start, end - start);
	return true;
}

void ConvertGlslDeclarations(std::string& text, DefineList& defines,
	std::vector<std::string>& instances)
{
	size_t index = 0;
	int depth = 0;
	bool lineStart = true;

	while (index < text.size())
	{
		const char character = text[index];

		if (character == '#' && lineStart && depth == 0)
		{
			index = SkipDirectiveLine(text, index);
			continue;
		}

		if (character == '\n')
		{
			lineStart = true;
			++index;
			continue;
		}

		if (!IsSpace(character))
			lineStart = false;

		if (character == '{')
		{
			++depth;
			++index;
			continue;
		}

		if (character == '}')
		{
			--depth;
			++index;
			continue;
		}

		if (depth > 0 || !IsWordChar(character) || (index > 0 && IsWordChar(text[index - 1])))
		{
			++index;
			continue;
		}

		size_t cursor = index;
		const std::string word = ReadWord(text, cursor);

		if (word == "uniform" && ConvertUniformDeclaration(text, index, cursor, defines, instances))
			continue;

		if (IsVaryingKeyword(word) &&
			ConvertVaryingDeclaration(text, index, cursor, defines, false))
		{
			continue;
		}

		if (word == "out" && ConvertVaryingDeclaration(text, index, cursor, defines, true))
			continue;

		index = cursor > index ? cursor : index + 1;
	}
}

bool RenameGlslMain(std::string& text)
{
	static const char* const kHeader =
		"GlslFragment(float2 FragCoordUv, out float4 FragColourOut)";

	size_t index = 0;
	bool renamed = false;

	while ((index = FindWord(text, "main", index)) != std::string::npos)
	{
		const size_t open = SkipSpace(text, index + 4);

		if (open >= text.size() || text[open] != '(')
		{
			index += 4;
			continue;
		}

		const size_t close = MatchBracket(text, open, '(', ')');

		if (close == std::string::npos)
			break;

		const size_t after = SkipSpace(text, close + 1);

		if (after >= text.size() || text[after] != '{')
		{
			index = close;
			continue;
		}

		text.replace(index, close + 1 - index, kHeader);
		index += strlen(kHeader);
		renamed = true;
	}

	return renamed;
}

int TopLevelCommas(const std::string& text, size_t open, size_t close)
{
	int depth = 0;
	int commas = 0;

	for (size_t index = open; index < close; ++index)
	{
		if (text[index] == '(' || text[index] == '[')
			++depth;
		else if (text[index] == ')' || text[index] == ']')
			--depth;
		else if (text[index] == ',' && depth == 1)
			++commas;
	}

	return commas;
}

void FixSplatConstructors(std::string& text)
{
	static const char* const kTypes[] = { "float2", "float3", "float4", "int2", "int3", "int4",
		"uint2", "uint3", "uint4", "bool2", "bool3", "bool4", "float2x2", "float3x3", "float4x4" };

	for (const char* const type : kTypes)
	{
		const size_t length = strlen(type);
		size_t index = 0;

		while ((index = FindWord(text, type, index)) != std::string::npos)
		{
			const size_t open = index + length;

			if (open >= text.size() || text[open] != '(')
			{
				index += length;
				continue;
			}

			const size_t close = MatchBracket(text, open, '(', ')');

			if (close == std::string::npos)
				break;

			if (TopLevelCommas(text, open, close) != 0)
			{
				index = close;
				continue;
			}

			text.insert(open, ")");
			text.insert(index, "(");
			index = close + 2;
		}
	}
}

void RenameReservedWords(std::string& text)
{
	static const char* const kReserved[] = { "line", "point", "triangle", "sample", "linear",
		"centroid", "column_major", "row_major", "half", "matrix", "vector", "string", "pass",
		"technique", "compile", "interface", "shared", "volatile", "snorm", "unorm",
		"pixelshader", "vertexshader", "stateblock" };

	for (const char* const reserved : kReserved)
	{
		const std::string renamed = std::string(reserved) + "_";

		ReplaceWord(text, reserved, renamed.c_str());
	}
}

void TranslateGlslWords(std::string& text)
{
	for (const Word& word : kGlslWords)
		ReplaceWord(text, word.from, word.to);

	RenameReservedWords(text);
	FixSplatConstructors(text);
}

bool TranslateGlsl(const std::string& source, std::string& outHlsl, std::string& outNote)
{
	std::string text = StripComments(source);
	DefineList defines;
	std::vector<std::string> instances;

	SelectFragmentStage(text);
	RemoveLayoutQualifiers(text);
	text = FilterGlslLines(text, defines);
	ConvertGlslDeclarations(text, defines, instances);

	for (const std::string& instance : instances)
		ReplaceAll(text, (instance + ".").c_str(), "");

	const bool shaderToy = FindWord(text, "mainImage", 0) != std::string::npos;

	if (FindWord(text, "gl_FragColor", 0) != std::string::npos)
		AddDefine(defines, "gl_FragColor", "FragColourOut");

	if (!shaderToy)
		ReplaceWord(text, "gl_FragCoord", "float4(FragCoordUv * FrameSize.zw, 0.0, 1.0)");

	for (const Named& known : kLibretroNames)
		AddDefine(defines, known.name, known.value);

	if (shaderToy)
	{
		for (const Named& known : kShaderToyNames)
			AddDefine(defines, known.name, known.value);
	}

	TranslateGlslWords(text);

	std::string entry;

	if (shaderToy)
	{
		entry = "\nfloat4 main(float2 uv : TEXCOORD0) : COLOR0\n{\n"
			"\tfloat4 outColour = float4(0.0, 0.0, 0.0, 1.0);\n"
			"\tmainImage(outColour, float2(uv.x, 1.0 - uv.y) * FrameSize.zw);\n"
			"\treturn outColour;\n}\n";

		outNote = "Shadertoy GLSL, mainImage over the frame";
	}
	else
	{
		if (!RenameGlslMain(text))
		{
			outNote = "no fragment main was found, so there is no pass to run";
			return false;
		}

		entry = "\nfloat4 main(float2 uv : TEXCOORD0) : COLOR0\n{\n"
			"\tfloat4 FragColourOut = float4(0.0, 0.0, 0.0, 1.0);\n"
			"\tGlslFragment(uv, FragColourOut);\n"
			"\treturn FragColourOut;\n}\n";

		outNote = "GLSL fragment stage, one pass";
	}

	outHlsl = std::string(kBindings) + kGlslPrologue + DefineText(defines) + "\n" + text + entry;
	return true;
}

}

int ShaderSource::ExtensionCount()
{
	return kExtensionCount;
}

const char* ShaderSource::ExtensionAt(int index)
{
	if (index < 0 || index >= kExtensionCount)
		return "";

	return kExtensions[index].text;
}

ShaderSource::Format ShaderSource::DetectFormat(const char* fileName)
{
	if (fileName == nullptr)
		return Format_Hlsl;

	const size_t length = strlen(fileName);

	for (const Extension& extension : kExtensions)
	{
		const size_t size = strlen(extension.text);

		if (length <= size)
			continue;

		if (_stricmp(fileName + length - size, extension.text) == 0)
			return extension.format;
	}

	return Format_Hlsl;
}

const char* ShaderSource::FormatName(Format format)
{
	if (format == Format_ReShade)
		return "ReShade";

	if (format == Format_Glsl)
		return "GLSL";

	return "HLSL";
}

bool ShaderSource::IsNative(Format format)
{
	return format == Format_Hlsl;
}

bool ShaderSource::Translate(Format format, const std::string& source, std::string& outHlsl,
	std::string& outNote)
{
	outHlsl.clear();
	outNote.clear();

	const bool hasMark = source.compare(0, 3, "\xef\xbb\xbf") == 0;
	const std::string text = hasMark ? source.substr(3) : source;

	if (format == Format_ReShade)
		return TranslateReShade(text, outHlsl, outNote);

	if (format == Format_Glsl)
		return TranslateGlsl(text, outHlsl, outNote);

	outHlsl = text;
	return true;
}
