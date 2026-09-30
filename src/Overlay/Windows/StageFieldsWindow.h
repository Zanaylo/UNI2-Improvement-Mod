#pragma once

#include "Overlay/Framework/IWindow.h"

#include <string>
#include <vector>

class StageFieldsWindow : public IWindow
{
public:
	StageFieldsWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

	void Show(int id, int slot, const std::string& name);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	void Refresh();
	bool GroupHeader(const char* group, bool edited);
	bool GroupEdited(const char* group) const;
	void ResetGroup(const char* group);
	void DrawCamera();
	void DrawField(int index);
	void Commit(int index, const std::string& value);
	void DrawResetAll();

	int m_id = -1;
	int m_slot = -1;
	bool m_focus = false;
	long m_revision = -1;
	std::vector<std::string> m_values;
	std::vector<bool> m_forced;
	std::vector<std::string> m_edited;
};
