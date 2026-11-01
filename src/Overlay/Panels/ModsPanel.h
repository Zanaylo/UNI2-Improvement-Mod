#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/Files/ModPacks.h"
#include "Overlay/Panels/ModDetails.h"
#include "Overlay/Panels/ModProfileBar.h"

#include <imgui.h>

#include <string>
#include <vector>

class ModsPanel
{
public:
	void Draw();
	void OnClosed();

private:
	void TakeInstalls();
	void Install(const std::string& source);

	void DrawTools();
	void DrawListTools();
	void ProcessQueue();
	void DrawList(const ImVec2& size);
	void DrawUserRow();
	void DrawRow(int index);
	void DrawRowBadges(const ModPacks::Pack& pack);
	void DrawRowMenu(int index);
	void DrawDragAndDrop(int index, const ModPacks::Pack& pack);
	void ApplyMove();
	void DrawDetails();
	void DrawFooter();
	void DrawHelp();

	bool Matches(const ModPacks::Pack& pack) const;
	int SelectedIndex() const;
	void MarkChanged();

	AsyncFileDialog m_zipDialog;
	AsyncFileDialog m_folderDialog;
	ModDetails m_details;
	ModProfileBar m_profiles;
	ImGuiTextFilter m_search;
	std::string m_selected;
	std::vector<std::string> m_queue;
	char m_status[192] = {};
	int m_moveFrom = -1;
	int m_moveTo = -1;
	bool m_dirty = false;
	bool m_changed = false;
};
