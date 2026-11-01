#pragma once

#include "Game/Files/ModPacks.h"

#include <imgui.h>

#include <string>
#include <vector>

class ModDetails
{
public:
	bool Draw(int index, char* status, int statusSize);
	void Release();

private:
	void Refresh(int index);

	void DrawAbout(const ModPacks::Pack& pack);
	bool DrawProblems(int index, const ModPacks::Pack& pack, char* status, int statusSize);
	bool DrawIssue(const ModRules::Issue& issue);
	void DrawStages(int index, const ModPacks::Pack& pack, char* status, int statusSize);
	void DrawMusic(const ModPacks::Pack& pack);
	bool DrawRemove(int index, const ModPacks::Pack& pack, char* status, int statusSize);
	void DrawFiles();
	void DrawConflicts();
	void DrawClashList(const char* id, const std::vector<ModPacks::Clash>& clashes);

	std::vector<std::string> m_files;
	std::vector<ModPacks::Clash> m_won;
	std::vector<ModPacks::Clash> m_lost;
	std::vector<int> m_shown;
	ImGuiTextFilter m_filter;
	long m_revision = -1;
	int m_index = -1;
	bool m_filesDirty = true;
};
