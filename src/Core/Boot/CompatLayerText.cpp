#include "Core/Boot/CompatLayerText.h"

#include <cwctype>
#include <sstream>
#include <vector>

namespace {

constexpr wchar_t kMarkerCharacters[] = L"~$^#";

std::vector<std::wstring> Tokens(const std::wstring& layers)
{
	std::wistringstream stream(layers);
	std::vector<std::wstring> tokens;
	std::wstring token;

	while (stream >> token)
		tokens.push_back(token);

	return tokens;
}

bool SameName(const std::wstring& token, const wchar_t* layer)
{
	const std::wstring wanted(layer);

	if (token.size() != wanted.size())
		return false;

	for (size_t i = 0; i < token.size(); ++i)
	{
		if (std::towupper(token[i]) != std::towupper(wanted[i]))
			return false;
	}

	return true;
}

bool IsMarker(const std::wstring& token)
{
	return token.find_first_not_of(kMarkerCharacters) == std::wstring::npos;
}

}

bool CompatLayerText::Contains(const std::wstring& layers, const wchar_t* layer)
{
	for (const std::wstring& token : Tokens(layers))
	{
		if (SameName(token, layer))
			return true;
	}

	return false;
}

std::wstring CompatLayerText::Without(const std::wstring& layers, const wchar_t* layer)
{
	std::wstring kept;
	bool keptALayer = false;

	for (const std::wstring& token : Tokens(layers))
	{
		if (SameName(token, layer))
			continue;

		keptALayer = keptALayer || !IsMarker(token);

		if (!kept.empty())
			kept += L' ';

		kept += token;
	}

	if (!keptALayer)
		return {};

	return kept;
}
