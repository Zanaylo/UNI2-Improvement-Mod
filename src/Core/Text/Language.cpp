#include "Core/Text/Language.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/Text/Catalogs/MainCatalogs.h"
#include "Core/Text/LabelText.h"
#include "Core/logger.h"

#include <Windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace {

constexpr int kLabelSlots = 16;
constexpr size_t kLabelBytes = 512;
constexpr size_t kFormatBytes = 1024;
const char* const kSettingSection = "Mod";
const char* const kSettingKey = "Language";

LanguageCatalog g_catalogs[kLanguageCount];
std::atomic<LanguageId> g_current{ LanguageId::English };
LanguagePick::Choice g_chosen = LanguagePick::Choice::Automatic;

const LanguageCatalog* CurrentCatalog()
{
	const LanguageId current = g_current.load();

	return current == LanguageId::English ? nullptr : &g_catalogs[static_cast<int>(current)];
}

char* NextLabelSlot()
{
	thread_local char slots[kLabelSlots][kLabelBytes];
	thread_local int next = 0;

	next = (next + 1) % kLabelSlots;
	return slots[next];
}

void Apply(LanguagePick::Choice choice)
{
	g_chosen = choice;
	g_current.store(LanguagePick::Resolve(choice, GetUserDefaultUILanguage()));
}

}

void Language::Initialize()
{
	AddCatalog(LanguageId::Portuguese, MainCatalogs::Portuguese());
	AddCatalog(LanguageId::Spanish, MainCatalogs::Spanish());
	Apply(LanguagePick::FromSetting(g_settings.language));

	LOG("Language: %s (setting '%s', Windows UI language 0x%04x), %zu Portuguese and %zu Spanish text(s)",
		LanguagePick::NameOf(LanguagePick::ChoiceOf(g_current.load())),
		g_settings.language.c_str(), static_cast<unsigned>(GetUserDefaultUILanguage()),
		g_catalogs[static_cast<int>(LanguageId::Portuguese)].Size(),
		g_catalogs[static_cast<int>(LanguageId::Spanish)].Size());
}

void Language::AddCatalog(LanguageId language, const LanguageCatalog::Table& table)
{
	if (language == LanguageId::English)
		return;

	g_catalogs[static_cast<int>(language)].Add(table);
}

LanguagePick::Choice Language::Chosen()
{
	return g_chosen;
}

void Language::Choose(LanguagePick::Choice choice)
{
	if (choice == g_chosen)
		return;

	g_settings.language = LanguagePick::SettingOf(choice);
	Settings::SaveString(kSettingSection, kSettingKey, g_settings.language.c_str());
	Apply(choice);
}

LanguageId Language::Current()
{
	return g_current.load();
}

const char* Tr(const char* english)
{
	const LanguageCatalog* const catalog = CurrentCatalog();

	if (catalog == nullptr || english == nullptr)
		return english;

	const char* const text = catalog->Find(english);

	return text != nullptr ? text : english;
}

const char* Tr(const std::string& english)
{
	return Tr(english.c_str());
}

const char* TrLabel(const char* label)
{
	const LanguageCatalog* const catalog = CurrentCatalog();

	if (catalog == nullptr || label == nullptr)
		return label;

	const char* const shown = catalog->Find(LabelText::Visible(label));

	if (shown == nullptr)
		return label;

	char* const slot = NextLabelSlot();

	return LabelText::Compose(shown, label, slot, kLabelBytes) ? slot : label;
}

std::string TrFormat(const char* english, ...)
{
	char text[kFormatBytes] = {};

	va_list arguments;
	va_start(arguments, english);
	vsnprintf(text, sizeof(text), Tr(english), arguments);
	va_end(arguments);

	return text;
}
