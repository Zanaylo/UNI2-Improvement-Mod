#include "Palette/PaletteSearch.h"

#include <cctype>

namespace {

char Lower(char letter)
{
	return static_cast<char>(tolower(static_cast<unsigned char>(letter)));
}

bool StartsWith(const char* text, const char* filter)
{
	for (; *filter != '\0'; ++text, ++filter)
	{
		if (*text == '\0' || Lower(*text) != Lower(*filter))
			return false;
	}

	return true;
}

}

bool PaletteSearch::Matches(const char* text, const char* filter)
{
	if (*filter == '\0')
		return true;

	for (; *text != '\0'; ++text)
	{
		if (StartsWith(text, filter))
			return true;
	}

	return false;
}
