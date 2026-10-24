#include "Core/Boot/StandbyRule.h"

namespace {

constexpr unsigned long long kNeverChecked = 0;

}

bool StandbyRule::Applies(GameBuild::Age age, bool allowUnsupported)
{
	if (allowUnsupported)
		return false;

	return age != GameBuild::Age::Supported;
}

bool StandbyRule::RecheckDue(unsigned long long nowMs, unsigned long long lastMs, unsigned long long intervalMs)
{
	if (lastMs == kNeverChecked)
		return true;

	return nowMs - lastMs >= intervalMs;
}
