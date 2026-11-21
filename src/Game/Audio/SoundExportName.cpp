#include "Game/Audio/SoundExportName.h"

#include <cstring>

namespace {

size_t ExtensionAt(const std::string& path)
{
	const size_t dot = path.find_last_of('.');
	const size_t slash = path.find_last_of("\\/");

	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return std::string::npos;

	return dot;
}

}

std::string SoundExportName::For(const std::string& target, const std::string& source)
{
	if (target.empty())
		return target;

	const size_t sourceDot = ExtensionAt(source);

	if (sourceDot == std::string::npos)
		return target;

	const std::string extension = source.substr(sourceDot);
	const size_t targetDot = ExtensionAt(target);

	if (targetDot == std::string::npos)
		return target + extension;

	if (_stricmp(target.c_str() + targetDot, extension.c_str()) == 0)
		return target;

	return target.substr(0, targetDot) + extension;
}
