#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/Files/ModPacks.h"

class ModsPanel
{
public:
	void Draw();

private:
	void DrawTools();
	void DrawList();
	void DrawUserRow();
	void DrawRow(int index);
	void DrawStages(int index, const ModPacks::Pack& pack);
	void DrawStageClash(int index, int number);
	void DrawStageInstall(const ModPacks::Pack& pack, int number);
	void DrawMusic(const ModPacks::Pack& pack);
	void DrawFooter();
	void DrawHelp();

	AsyncFileDialog m_install;
	char m_status[192] = {};
	int m_moved = -1;
	int m_delta = 0;
	bool m_dirty = false;
	bool m_changed = false;
};
