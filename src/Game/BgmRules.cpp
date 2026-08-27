#include "Game/BgmRules.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmTable.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

BgmRules::Rule g_rules[BgmRules::kMaxRules] = {};
int g_count = 0;
bool g_enabled = true;

std::string IniPath()
{
	return GetModRootPath("bgm.ini");
}

void SectionName(int index, char* out, size_t size)
{
	sprintf_s(out, size, "Rule%d", index);
}

int ReadInt(const char* section, const char* key, int fallback, const std::string& path)
{
	return static_cast<int>(GetPrivateProfileIntA(section, key, fallback, path.c_str()));
}

void WriteInt(const char* section, const char* key, int value, const std::string& path)
{
	char buffer[24] = {};
	sprintf_s(buffer, "%d", value);
	WritePrivateProfileStringA(section, key, buffer, path.c_str());
}

int ReadRef(const char* section, const char* key, const std::string& path)
{
	char buffer[64] = {};
	GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path.c_str());
	return BgmLibrary::ParseRef(buffer);
}

void WriteRef(const char* section, const char* key, int value, const std::string& path)
{
	char buffer[64] = {};
	BgmLibrary::FormatRef(value, buffer, sizeof(buffer));
	WritePrivateProfileStringA(section, key, buffer, path.c_str());
}

bool IsValidBgm(int id)
{
	if (id >= BgmLibrary::kFirstId)
		return id < BgmLibrary::kFirstId + BgmLibrary::kMaxTracks;

	return id >= 0 && id < BgmTable::kSlotCount;
}

bool IsValidSlot(int id)
{
	return id >= 0 && id < BgmTable::kSlotCount;
}

bool IsValidRule(const BgmRules::Rule& rule)
{
	if (rule.kind < 0 || rule.kind >= BgmRules::Kind_COUNT)
		return false;

	if (!IsValidBgm(rule.bgm))
		return false;

	if (rule.kind == BgmRules::Kind_Replace)
		return IsValidSlot(rule.a);

	if (rule.a < 0)
		return false;

	if (rule.kind == BgmRules::Kind_Matchup && rule.b < 0)
		return false;

	return true;
}

bool IsBattleSlot(int id)
{
	return (id >= 1 && id <= 27) || (id >= 91 && id <= 93);
}

bool MatchupApplies(const BgmRules::Rule& rule, int left, int right)
{
	if (left < 0 || right < 0)
		return false;

	if (rule.a == left && rule.b == right)
		return true;

	return rule.bothWays && rule.a == right && rule.b == left;
}

bool CharacterApplies(const BgmRules::Rule& rule, int left, int right)
{
	return rule.a == left || rule.a == right;
}

int ResolveKind(int kind, int requestedBgm, int left, int right)
{
	for (int i = 0; i < g_count; ++i)
	{
		const BgmRules::Rule& rule = g_rules[i];

		if (!rule.enabled || rule.kind != kind)
			continue;

		if (kind == BgmRules::Kind_Matchup && !MatchupApplies(rule, left, right))
			continue;

		if (kind == BgmRules::Kind_Character && !CharacterApplies(rule, left, right))
			continue;

		if (kind == BgmRules::Kind_Replace && rule.a != requestedBgm)
			continue;

		return rule.bgm;
	}

	return -1;
}

}

void BgmRules::Load()
{
	const std::string path = IniPath();

	g_count = 0;
	g_enabled = ReadInt("Bgm", "RulesEnabled", 1, path) != 0;

	const int stored = ReadInt("Bgm", "RuleCount", 0, path);

	for (int i = 0; i < stored && g_count < kMaxRules; ++i)
	{
		char section[24] = {};
		SectionName(i, section, sizeof(section));

		Rule rule = {};
		rule.kind = ReadInt(section, "Kind", -1, path);
		rule.a = ReadInt(section, "A", -1, path);
		rule.b = ReadInt(section, "B", -1, path);
		rule.bgm = ReadRef(section, "Bgm", path);
		rule.bothWays = ReadInt(section, "BothWays", 1, path) != 0;
		rule.enabled = ReadInt(section, "Enabled", 1, path) != 0;
		rule.fromTheme = ReadInt(section, "FromTheme", 0, path) != 0;

		if (!IsValidRule(rule))
			continue;

		g_rules[g_count] = rule;
		++g_count;
	}

	LOG("BgmRules: %d rule(s) loaded, rules are %s", g_count, g_enabled ? "on" : "off");
}

void BgmRules::Save()
{
	const std::string path = IniPath();

	WriteInt("Bgm", "RulesEnabled", g_enabled ? 1 : 0, path);
	WriteInt("Bgm", "RuleCount", g_count, path);

	for (int i = 0; i < kMaxRules; ++i)
	{
		char section[24] = {};
		SectionName(i, section, sizeof(section));

		if (i >= g_count)
		{
			WritePrivateProfileStringA(section, nullptr, nullptr, path.c_str());
			continue;
		}

		const Rule& rule = g_rules[i];
		WriteInt(section, "Kind", rule.kind, path);
		WriteInt(section, "A", rule.a, path);
		WriteInt(section, "B", rule.b, path);
		WriteRef(section, "Bgm", rule.bgm, path);
		WriteInt(section, "BothWays", rule.bothWays ? 1 : 0, path);
		WriteInt(section, "Enabled", rule.enabled ? 1 : 0, path);
		WriteInt(section, "FromTheme", rule.fromTheme ? 1 : 0, path);
	}
}

bool BgmRules::IsEnabled()
{
	return g_enabled;
}

void BgmRules::SetEnabled(bool enabled)
{
	g_enabled = enabled;
}

int BgmRules::Count()
{
	return g_count;
}

const BgmRules::Rule* BgmRules::Get(int index)
{
	if (index < 0 || index >= g_count)
		return nullptr;

	return &g_rules[index];
}

bool BgmRules::Add(const Rule& rule)
{
	if (g_count >= kMaxRules || !IsValidRule(rule))
		return false;

	g_rules[g_count] = rule;
	++g_count;
	return true;
}

bool BgmRules::Update(int index, const Rule& rule)
{
	if (index < 0 || index >= g_count || !IsValidRule(rule))
		return false;

	g_rules[index] = rule;
	return true;
}

void BgmRules::RemoveThemeRules()
{
	int kept = 0;

	for (int i = 0; i < g_count; ++i)
	{
		if (g_rules[i].fromTheme)
			continue;

		g_rules[kept] = g_rules[i];
		++kept;
	}

	g_count = kept;
}

bool BgmRules::Remove(int index)
{
	if (index < 0 || index >= g_count)
		return false;

	for (int i = index; i + 1 < g_count; ++i)
		g_rules[i] = g_rules[i + 1];

	--g_count;
	return true;
}

bool BgmRules::ExportTo(const char* path)
{
	if (path == nullptr || path[0] == 0)
		return false;

	FILE* handle = nullptr;

	if (fopen_s(&handle, path, "wb") != 0 || handle == nullptr)
		return false;

	fprintf(handle, "#rules 1\r\n");

	for (int i = 0; i < g_count; ++i)
	{
		const Rule& rule = g_rules[i];

		char ref[64] = {};
		BgmLibrary::FormatRef(rule.bgm, ref, sizeof(ref));

		fprintf(handle, "%d|%d|%d|%s|%d|%d\r\n", rule.kind, rule.a, rule.b, ref,
			rule.bothWays ? 1 : 0, rule.enabled ? 1 : 0);
	}

	fclose(handle);
	LOG("BgmRules: exported %d rule(s) to %s", g_count, path);
	return true;
}

int BgmRules::ImportFrom(const char* path)
{
	if (path == nullptr || path[0] == 0)
		return 0;

	FILE* handle = nullptr;

	if (fopen_s(&handle, path, "rb") != 0 || handle == nullptr)
		return 0;

	int added = 0;
	char line[256] = {};

	while (fgets(line, sizeof(line), handle) != nullptr)
	{
		if (line[0] == '#' || line[0] == ';')
			continue;

		char ref[64] = {};
		int kind = -1;
		int a = -1;
		int b = -1;
		int bothWays = 1;
		int enabled = 1;

		if (sscanf_s(line, "%d|%d|%d|%63[^|]|%d|%d", &kind, &a, &b, ref,
			static_cast<unsigned>(sizeof(ref)), &bothWays, &enabled) != 6)
		{
			continue;
		}

		Rule rule = {};
		rule.kind = kind;
		rule.a = a;
		rule.b = b;
		rule.bgm = BgmLibrary::ParseRef(ref);
		rule.bothWays = bothWays != 0;
		rule.enabled = enabled != 0;
		rule.fromTheme = false;

		if (Add(rule))
			++added;
	}

	fclose(handle);

	if (added > 0)
		Save();

	LOG("BgmRules: imported %d rule(s) from %s", added, path);
	return added;
}

int BgmRules::Resolve(int requestedBgm, int charaLeft, int charaRight)
{
	if (!g_enabled || g_count == 0)
		return -1;

	const int order[] = { Kind_Matchup, Kind_Character, Kind_Replace };

	const bool battle = IsBattleSlot(requestedBgm);

	for (int kind : order)
	{
		if (kind != Kind_Replace && !battle)
			continue;

		const int resolved = ResolveKind(kind, requestedBgm, charaLeft, charaRight);

		if (resolved < 0 || resolved == requestedBgm)
			continue;

		if (!BgmLibrary::IsPlayable(resolved))
			continue;

		return resolved;
	}

	return -1;
}

const char* BgmRules::KindName(int kind)
{
	switch (kind)
	{
	case Kind_Matchup:
		return "Matchup";
	case Kind_Character:
		return "Character";
	case Kind_Replace:
		return "Replace track";
	default:
		return "?";
	}
}
