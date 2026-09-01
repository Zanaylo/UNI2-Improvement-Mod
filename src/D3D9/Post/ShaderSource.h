#pragma once

#include <string>

namespace ShaderSource
{
	enum Format
	{
		Format_Hlsl,
		Format_ReShade,
		Format_Glsl,
		Format_Count
	};

	int ExtensionCount();
	const char* ExtensionAt(int index);

	Format DetectFormat(const char* fileName);
	const char* FormatName(Format format);
	bool IsNative(Format format);

	bool Translate(Format format, const std::string& source, std::string& outHlsl,
		std::string& outNote);
}
