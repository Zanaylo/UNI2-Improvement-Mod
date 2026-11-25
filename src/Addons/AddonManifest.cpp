#include "Addons/AddonManifest.h"

#include <cctype>

namespace {

std::string Trim(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t\r\n");

	if (first == std::string::npos)
		return std::string();

	const size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

bool SameNoCase(const std::string& left, const char* right)
{
	const std::string other(right);

	if (left.size() != other.size())
		return false;

	for (size_t i = 0; i < left.size(); ++i)
	{
		if (tolower(static_cast<unsigned char>(left[i])) != tolower(static_cast<unsigned char>(other[i])))
			return false;
	}

	return true;
}

void AppendTokens(const std::string& value, std::vector<std::string>& out)
{
	size_t pos = 0;

	while (pos < value.size())
	{
		const size_t start = value.find_first_not_of(" \t,|;", pos);

		if (start == std::string::npos)
			break;

		const size_t end = value.find_first_of(" \t,|;", start);
		const std::string token = Trim(end == std::string::npos
			? value.substr(start)
			: value.substr(start, end - start));

		if (!token.empty())
			out.push_back(token);

		pos = end == std::string::npos ? value.size() : end + 1;
	}
}

}

AddonManifest AddonManifest::Parse(const std::string& text, const std::string& fallbackId)
{
	AddonManifest manifest;
	manifest.id = fallbackId;
	manifest.name = fallbackId;

	bool inSection = true;
	size_t cursor = 0;

	while (cursor < text.size())
	{
		const size_t end = text.find('\n', cursor);
		const std::string raw = end == std::string::npos ? text.substr(cursor) : text.substr(cursor, end - cursor);
		cursor = end == std::string::npos ? text.size() : end + 1;

		const std::string line = Trim(raw);

		if (line.empty() || line[0] == ';' || line[0] == '#')
			continue;

		if (line.front() == '[' && line.back() == ']')
		{
			const std::string section = Trim(line.substr(1, line.size() - 2));
			inSection = SameNoCase(section, "Addon");
			continue;
		}

		if (!inSection)
			continue;

		const size_t eq = line.find('=');

		if (eq == std::string::npos)
			continue;

		const std::string key = Trim(line.substr(0, eq));
		const std::string value = Trim(line.substr(eq + 1));

		if (SameNoCase(key, "Id") && !value.empty())
			manifest.id = value;
		else if (SameNoCase(key, "Name") && !value.empty())
			manifest.name = value;
		else if (SameNoCase(key, "Version"))
			manifest.version = value;
		else if (SameNoCase(key, "Author"))
			manifest.author = value;
		else if (SameNoCase(key, "Description") || SameNoCase(key, "Note"))
			manifest.description = value;
		else if (SameNoCase(key, "Entry") || SameNoCase(key, "Script"))
			manifest.entry = value;
		else if (SameNoCase(key, "Native") || SameNoCase(key, "Dll"))
			manifest.nativeDll = value;
		else if (SameNoCase(key, "Url"))
			manifest.url = value;
		else if (SameNoCase(key, "Modules") || SameNoCase(key, "Module") || SameNoCase(key, "EnablesModule"))
			AppendTokens(value, manifest.modules);
		else if (SameNoCase(key, "Assets"))
			manifest.assets = value;
	}

	return manifest;
}
