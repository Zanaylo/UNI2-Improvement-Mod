#pragma once

#include <string>

namespace UpdaterPaths
{
	std::wstring Combine(const std::wstring& folder, const std::wstring& name);
	std::wstring Parent(const std::wstring& path);

	bool Exists(const std::wstring& path);
	bool EnsureFolder(const std::wstring& path);
	bool Same(const std::wstring& left, const std::wstring& right);

	std::wstring Self();

	std::string Utf8(const std::wstring& value);
	std::string Ansi(const std::wstring& value);
	std::wstring Wide(const std::string& utf8);

	std::string Stamp();
}
