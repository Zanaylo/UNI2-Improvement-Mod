#include "Game/Debug/ScriptDefines.h"

#include <cctype>
#include <cstring>

namespace {

constexpr const char* kConstKeyword = "const";
constexpr const char* kNeverSet = "Def_Dbg_LocalDebugScriptPath";

bool IsNameCharacter(char letter)
{
	return std::isalnum(static_cast<unsigned char>(letter)) != 0 || letter == '_';
}

size_t SkipBlanks(const std::string& text, size_t at)
{
	while (at < text.size() && (text[at] == ' ' || text[at] == '\t'))
		++at;

	return at;
}

bool FindDeclaration(const std::string& text, const char* name, size_t& valueStart, size_t& valueEnd)
{
	const size_t nameLength = strlen(name);

	for (size_t at = text.find(name); at != std::string::npos; at = text.find(name, at + 1))
	{
		const size_t after = at + nameLength;

		if (after < text.size() && IsNameCharacter(text[after]))
			continue;

		const size_t keyword = text.rfind(kConstKeyword, at);

		if (keyword == std::string::npos || SkipBlanks(text, keyword + strlen(kConstKeyword)) != at)
			continue;

		const size_t equals = SkipBlanks(text, after);

		if (equals >= text.size() || text[equals] != '=')
			continue;

		valueStart = SkipBlanks(text, equals + 1);
		valueEnd = valueStart;

		while (valueEnd < text.size() && (text[valueEnd] == '-' || std::isdigit(static_cast<unsigned char>(text[valueEnd]))))
			++valueEnd;

		return valueEnd > valueStart;
	}

	return false;
}

}

bool ScriptDefines::Rewrite(const std::string& source, const Value* values, int count, std::string& out,
	std::string& missing)
{
	out = source;
	missing.clear();

	for (int i = 0; i < count; ++i)
	{
		size_t valueStart = 0;
		size_t valueEnd = 0;

		if (strcmp(values[i].name, kNeverSet) == 0 || !FindDeclaration(out, values[i].name, valueStart, valueEnd))
		{
			missing = values[i].name;
			return false;
		}

		out.replace(valueStart, valueEnd - valueStart, std::to_string(values[i].value));
	}

	return true;
}
