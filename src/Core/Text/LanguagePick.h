#pragma once

#include "Core/Text/LanguageId.h"

#include <string>

namespace LanguagePick
{
	enum class Choice
	{
		Automatic,
		English,
		Portuguese,
		Spanish
	};

	constexpr int kChoiceCount = 4;

	Choice FromSetting(const std::string& value);
	const char* SettingOf(Choice choice);
	const char* NameOf(Choice choice);

	Choice ChoiceOf(LanguageId language);

	LanguageId FromSystem(unsigned short languageId);
	LanguageId Resolve(Choice choice, unsigned short systemLanguageId);
}
