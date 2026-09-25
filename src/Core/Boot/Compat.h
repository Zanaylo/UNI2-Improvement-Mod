#pragma once

namespace Compat
{
	void Detect();

	bool IsWine();
	const char* WineVersion();
	const char* HostSystem();
	bool IsRtssPresent();

	bool SafeMode();

	void StandDown();
	bool StoodDown();

	const char* Describe();
}
