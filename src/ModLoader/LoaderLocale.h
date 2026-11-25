#pragma once

#include "Core/Config/FeatureCatalog.h"

#include <string>

namespace LoaderLocale
{
	enum class Lang
	{
		En = 0,
		PtBr = 1,
		EsLatam = 2
	};

	void Initialize(const std::string& gameDir);
	Lang Current();
	const char* Code(Lang lang);
	void Set(Lang lang);

	const char* Text(const char* key);
	const char* ModuleName(FeatureCatalog::Id id);
	const char* ModuleSummary(FeatureCatalog::Id id);
}
