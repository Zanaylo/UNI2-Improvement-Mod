#pragma once

#include "Core/GameBuild.h"

namespace StandbyRule
{
	bool Applies(GameBuild::Age age, bool allowUnsupported);
	bool RecheckDue(unsigned long long nowMs, unsigned long long lastMs, unsigned long long intervalMs);
}
