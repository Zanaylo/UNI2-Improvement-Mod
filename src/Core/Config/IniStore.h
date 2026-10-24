#pragma once

#include <Windows.h>

#include <string>

namespace Ini
{
	using SectionRouter = const char* (*)(const char* section);

	void Route(const std::string& legacyPath, const std::string& root, SectionRouter router);
	std::string PathFor(const char* section, const char* path);

	DWORD GetString(const char* section, const char* key, const char* fallback, char* out, DWORD size,
		const char* path);
	UINT GetInt(const char* section, const char* key, int fallback, const char* path);
	DWORD GetSection(const char* section, char* out, DWORD size, const char* path);

	BOOL Write(const char* section, const char* key, const char* value, const char* path);

	void Flush();
	void Forget();
	void FlushOnExit();
}
