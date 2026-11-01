#include "Game/Files/ModManifest.h"

#include "Core/Text/TextList.h"

#include <cstring>

namespace {

constexpr const char* kSection = "mod";
constexpr const char* kListSeparators = ",;|";
constexpr const char* kByteOrderMark = "\xEF\xBB\xBF";
constexpr const char* kLineBreaks = "\r\n";

struct Field
{
	const char* key;
	std::string ModManifest::* text;
};

struct ListField
{
	const char* key;
	std::vector<std::string> ModManifest::* list;
};

const Field kFields[] = {
	{ "id", &ModManifest::id },
	{ "name", &ModManifest::name },
	{ "author", &ModManifest::author },
	{ "version", &ModManifest::version },
	{ "description", &ModManifest::description },
	{ "url", &ModManifest::url },
	{ "website", &ModManifest::url },
	{ "category", &ModManifest::category },
};

const ListField kListFields[] = {
	{ "requires", &ModManifest::requirements },
	{ "conflicts", &ModManifest::conflicts },
	{ "above", &ModManifest::above },
	{ "below", &ModManifest::below },
};

constexpr const char* kNoteKey = "note";

const char* const kWebSchemes[] = { "https://", "http://" };

bool IsComment(const std::string& line)
{
	return line.empty() || line[0] == ';' || line[0] == '#';
}

bool IsSection(const std::string& line)
{
	return line.size() >= 2 && line.front() == '[' && line.back() == ']';
}

bool Assign(ModManifest& manifest, const std::string& key, const std::string& value)
{
	for (const Field& field : kFields)
	{
		if (TextList::SameNoCase(key, field.key))
		{
			manifest.*field.text = value;
			return true;
		}
	}

	for (const ListField& field : kListFields)
	{
		if (TextList::SameNoCase(key, field.key))
		{
			manifest.*field.list = TextList::Split(value, kListSeparators);
			return true;
		}
	}

	return false;
}

std::string WithoutByteOrderMark(const std::string& text)
{
	return text.compare(0, 3, kByteOrderMark) == 0 ? text.substr(3) : text;
}

}

ModManifest ModManifest::Parse(const std::string& text, const std::string& folder)
{
	ModManifest manifest;
	std::string note;
	bool inside = false;

	for (const std::string& line : TextList::Split(WithoutByteOrderMark(text), kLineBreaks))
	{
		if (IsComment(line))
			continue;

		if (IsSection(line))
		{
			inside = TextList::SameNoCase(TextList::Trimmed(line.substr(1, line.size() - 2)), kSection);
			manifest.described = manifest.described || inside;
			continue;
		}

		const size_t equals = line.find('=');

		if (!inside || equals == std::string::npos)
			continue;

		const std::string key = TextList::Trimmed(line.substr(0, equals));
		const std::string value = TextList::Trimmed(line.substr(equals + 1));

		if (TextList::SameNoCase(key, kNoteKey))
			note = value;
		else
			Assign(manifest, key, value);
	}

	if (manifest.description.empty())
		manifest.description = note;

	if (manifest.id.empty())
		manifest.id = folder;

	if (manifest.name.empty())
		manifest.name = manifest.id;

	return manifest;
}

bool ModManifest::HasWebLink() const
{
	for (const char* scheme : kWebSchemes)
	{
		const size_t length = strlen(scheme);

		if (url.size() > length && _strnicmp(url.c_str(), scheme, length) == 0)
			return true;
	}

	return false;
}
