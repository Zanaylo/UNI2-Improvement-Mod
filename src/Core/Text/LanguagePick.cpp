#include "Core/Text/LanguagePick.h"

#include <cctype>

namespace {

using LanguagePick::Choice;

constexpr unsigned short kPrimaryMask = 0x3ff;
constexpr unsigned short kPrimaryPortuguese = 0x16;
constexpr unsigned short kPrimarySpanish = 0x0a;

struct Known
{
	Choice choice;
	const char* setting;
	const char* name;
};

const Known kKnown[] = {
	{ Choice::Automatic, "auto", "Automatic" },
	{ Choice::English, "en", "English" },
	{ Choice::Portuguese, "pt", u8"Português" },
	{ Choice::Spanish, "es", u8"Español" },
};

std::string Lowered(const std::string& value)
{
	std::string out;
	out.reserve(value.size());

	for (const char c : value)
	{
		if (std::isspace(static_cast<unsigned char>(c)))
			continue;

		out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	return out;
}

const Known& KnownOf(Choice choice)
{
	for (const Known& known : kKnown)
	{
		if (known.choice == choice)
			return known;
	}

	return kKnown[0];
}

}

Choice LanguagePick::FromSetting(const std::string& value)
{
	const std::string wanted = Lowered(value);

	for (const Known& known : kKnown)
	{
		if (wanted == known.setting)
			return known.choice;
	}

	return Choice::Automatic;
}

const char* LanguagePick::SettingOf(Choice choice)
{
	return KnownOf(choice).setting;
}

const char* LanguagePick::NameOf(Choice choice)
{
	return KnownOf(choice).name;
}

Choice LanguagePick::ChoiceOf(LanguageId language)
{
	switch (language)
	{
	case LanguageId::Portuguese:
		return Choice::Portuguese;
	case LanguageId::Spanish:
		return Choice::Spanish;
	default:
		return Choice::English;
	}
}

LanguageId LanguagePick::FromSystem(unsigned short languageId)
{
	const unsigned short primary = languageId & kPrimaryMask;

	if (primary == kPrimaryPortuguese)
		return LanguageId::Portuguese;

	if (primary == kPrimarySpanish)
		return LanguageId::Spanish;

	return LanguageId::English;
}

LanguageId LanguagePick::Resolve(Choice choice, unsigned short systemLanguageId)
{
	switch (choice)
	{
	case Choice::English:
		return LanguageId::English;
	case Choice::Portuguese:
		return LanguageId::Portuguese;
	case Choice::Spanish:
		return LanguageId::Spanish;
	default:
		return FromSystem(systemLanguageId);
	}
}
