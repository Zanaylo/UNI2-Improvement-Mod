#pragma once

#include "Core/Config/FeatureCatalog.h"

namespace Features
{
	void Freeze();
	void EnableTransient(FeatureCatalog::Id id);

	bool IsAvailable(FeatureCatalog::Id id);
	bool IsOn(FeatureCatalog::Id id);
	bool IsLoaded(FeatureCatalog::Id id);
	bool Shows(FeatureCatalog::Id id);

	void Set(FeatureCatalog::Id id, bool on);
	bool NeedsRestart();
}
