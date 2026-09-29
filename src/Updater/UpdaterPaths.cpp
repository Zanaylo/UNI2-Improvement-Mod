#include "Updater/UpdaterPaths.h"

#include <Windows.h>

#include <cstdio>
#include <ctime>

namespace {

std::string Narrow(const std::wstring& value, UINT codePage)
{
	if (value.empty())
		return std::string();

	const int length = WideCharToMultiByte(codePage, 0, value.c_str(), -1, nullptr, 0, nullptr,
		nullptr);

	if (length <= 1)
		return std::string();

	std::string text(static_cast<size_t>(length - 1), '\0');
	WideCharToMultiByte(codePage, 0, value.c_str(), -1, &text[0], length, nullptr, nullptr);
	return text;
}

std::wstring FullPath(const std::wstring& path)
{
	wchar_t buffer[MAX_PATH * 2] = {};

	const DWORD length = GetFullPathNameW(path.c_str(), MAX_PATH * 2, buffer, nullptr);

	return length == 0 || length >= MAX_PATH * 2 ? path : std::wstring(buffer, length);
}

}

std::wstring UpdaterPaths::Combine(const std::wstring& folder, const std::wstring& name)
{
	if (folder.empty())
		return name;

	if (name.empty())
		return folder;

	if (folder.back() == L'\\' || folder.back() == L'/')
		return folder + name;

	return folder + L"\\" + name;
}

std::wstring UpdaterPaths::Parent(const std::wstring& path)
{
	const size_t slash = path.find_last_of(L"\\/");

	return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

bool UpdaterPaths::Exists(const std::wstring& path)
{
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool UpdaterPaths::EnsureFolder(const std::wstring& path)
{
	if (path.empty() || Exists(path))
		return true;

	if (!EnsureFolder(Parent(path)))
		return false;

	return CreateDirectoryW(path.c_str(), nullptr) != FALSE ||
		GetLastError() == ERROR_ALREADY_EXISTS;
}

bool UpdaterPaths::Same(const std::wstring& left, const std::wstring& right)
{
	return _wcsicmp(FullPath(left).c_str(), FullPath(right).c_str()) == 0;
}

std::wstring UpdaterPaths::Self()
{
	wchar_t path[MAX_PATH * 2] = {};

	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH * 2);

	return length == 0 ? std::wstring() : std::wstring(path, length);
}

std::string UpdaterPaths::Utf8(const std::wstring& value)
{
	return Narrow(value, CP_UTF8);
}

std::string UpdaterPaths::Ansi(const std::wstring& value)
{
	return Narrow(value, CP_ACP);
}

std::wstring UpdaterPaths::Wide(const std::string& utf8)
{
	if (utf8.empty())
		return std::wstring();

	const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);

	if (length <= 1)
		return std::wstring();

	std::wstring wide(static_cast<size_t>(length - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], length);
	return wide;
}

std::string UpdaterPaths::Stamp()
{
	std::time_t now = std::time(nullptr);
	std::tm parts = {};
	gmtime_s(&parts, &now);

	char text[32] = {};
	std::snprintf(text, sizeof(text), "%04d%02d%02d-%02d%02d%02d", parts.tm_year + 1900,
		parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);

	return text;
}
