// Balance a patch changed inside the engine rather than in a file.
//
// The one rule so far is the minimum guaranteed damage. 1.05 returned floor * damage / 100; 1.10
// returns floor * damage * 80 / 10000, which is four fifths of it, and no file carries the
// difference. The mod hooks the routine the way it hooks everything else and runs 1.05's own
// arithmetic while a pre-1.10 patch is loaded - the detour passes straight through otherwise, and
// always online.
//
// An earlier attempt held the engine's unused HoseiBaseMinValue at 125 so the engine would cancel
// its own scaling. It does, to within a fifth of a percent: the engine truncates 125 * floor / 100
// before the multiply, and that rounding is visible in a real combo. Exact needs the whole
// expression, which needs the call.

#pragma once

namespace BalanceRules
{
	struct Rule
	{
		const char* id;
		const char* name;
		const char* detail;
		int since;
	};

	bool Install();

	int Count();
	const Rule* Get(int index);
	bool IsActive(int index);

	void SetVersion(int version);
	void Release();

	void OnFrame();

	const char* StatusText();
}
