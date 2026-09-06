#pragma once

#include "Core/AsyncFileDialog.h"

class ModsPanel
{
public:
	void Draw();

private:
	void DrawTools();
	void DrawList();
	void DrawOwnRow();
	void DrawRow(int index);
	void DrawFooter();
	void DrawHelp();

	AsyncFileDialog m_install;
	char m_status[192] = {};
	int m_moved = -1;
	int m_delta = 0;
	bool m_dirty = false;
};
