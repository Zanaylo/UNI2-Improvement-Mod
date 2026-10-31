#pragma once

#include "Core/Text/LanguageCatalog.h"
#include "Core/Text/LanguageId.h"
#include "Core/Text/LanguagePick.h"

#include <string>

namespace Language
{
	void Initialize();
	void AddCatalog(LanguageId language, const LanguageCatalog::Table& table);

	LanguagePick::Choice Chosen();
	void Choose(LanguagePick::Choice choice);
	LanguageId Current();
}

const char* Tr(const char* english);
const char* Tr(const std::string& english);
const char* TrLabel(const char* label);
std::string TrFormat(const char* english, ...);
