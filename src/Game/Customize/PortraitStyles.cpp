#include "Game/Customize/PortraitStyles.h"

#include <cstring>

namespace {

struct Style
{
	const char* name;
	const char* labels[4];
	const char* prefix;
};

const Style kStyles[] = {
	{ "Old Arts", { "UNDER NIGHT IN-BIRTH", "Exe:Late", "Exe:Late[st]", "Exe:Late[cl-r]" }, nullptr },
	{ "Sys:Celes", { "Sys:Celes" }, nullptr },
	{ "Sys:Celes, no effects", {}, "Sys:Celes, no " },
	{ "BBTAG", { "BBTAG" }, nullptr },
	{ "Victory", { "Victory" }, nullptr },
	{ "Story", { "Story" }, nullptr },
	{ "Chibi", { "Chibi" }, nullptr },
};

constexpr int kStyleCount = static_cast<int>(sizeof(kStyles) / sizeof(kStyles[0]));

bool HasPrefix(const char* label, const char* prefix)
{
	return prefix != nullptr && strncmp(label, prefix, strlen(prefix)) == 0;
}

bool Matches(const Style& style, const char* label)
{
	if (HasPrefix(label, style.prefix))
		return true;

	for (const char* wanted : style.labels)
	{
		if (wanted != nullptr && strcmp(label, wanted) == 0)
			return true;
	}

	return false;
}

}

int PortraitStyles::Count()
{
	return kStyleCount;
}

const char* PortraitStyles::Name(int style)
{
	return style >= 0 && style < kStyleCount ? kStyles[style].name : "";
}

const PortraitCatalog::Art* PortraitStyles::For(int chara, int style)
{
	if (style < 0 || style >= kStyleCount)
		return nullptr;

	for (const PortraitCatalog::Art* art : PortraitCatalog::Of(chara))
	{
		if (Matches(kStyles[style], art->label))
			return art;
	}

	return nullptr;
}
