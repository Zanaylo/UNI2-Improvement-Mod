#pragma once

#include <string>

namespace CompatLayerText
{
	bool Contains(const std::wstring& layers, const wchar_t* layer);
	std::wstring Without(const std::wstring& layers, const wchar_t* layer);
}
