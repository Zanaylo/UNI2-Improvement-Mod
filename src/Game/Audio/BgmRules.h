#pragma once

#include <string>
#include <vector>

namespace BgmRules
{
	constexpr int kMaxRules = 128;

	enum Kind
	{
		Kind_Matchup,
		Kind_Character,
		Kind_Replace,
		Kind_COUNT
	};

	struct Rule
	{
		int kind;
		int a;
		int b;
		int bgm;
		bool bothWays;
		bool enabled;
		bool fromTheme;
	};

	void Load();
	void Save();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	int Count();
	const Rule* Get(int index);

	bool Add(const Rule& rule);
	bool Update(int index, const Rule& rule);
	bool Remove(int index);
	void RemoveThemeRules();

	bool ExportTo(const char* path);
	int ImportFrom(const char* path);

	int Resolve(int requestedBgm, int charaLeft, int charaRight);

	void Remap(const std::vector<std::string>& before, const std::vector<std::string>& after);

	const char* KindName(int kind);
}
