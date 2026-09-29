#pragma once

#include <string>

namespace UpdaterLog
{
	void Open(const std::wstring& path);
	void Write(const char* format, ...);
}
