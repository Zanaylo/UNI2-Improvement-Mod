#include "Updater/UpdaterLog.h"

#include "Core/logger.h"
#include "Updater/UpdaterPaths.h"

#include <cstdarg>
#include <cstdio>

namespace {

std::wstring g_path;

void WriteList(const char* format, va_list args)
{
	if (g_path.empty())
		return;

	UpdaterPaths::EnsureFolder(UpdaterPaths::Parent(g_path));

	FILE* file = nullptr;

	if (_wfopen_s(&file, g_path.c_str(), L"ab") != 0 || file == nullptr)
		return;

	const std::string prefix = "[" + UpdaterPaths::Stamp() + "] ";
	fwrite(prefix.data(), 1, prefix.size(), file);
	vfprintf(file, format, args);
	fwrite("\r\n", 1, 2, file);
	fclose(file);
}

}

void UpdaterLog::Open(const std::wstring& path)
{
	g_path = path;
}

void UpdaterLog::Write(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	WriteList(format, args);
	va_end(args);
}

void WriteLog(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	WriteList(format, args);
	va_end(args);
}
