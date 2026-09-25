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
