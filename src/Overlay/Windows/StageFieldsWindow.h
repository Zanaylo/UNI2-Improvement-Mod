#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/Stages/StageInstall.h"
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
	void DrawExport();
	void DrawGamePicker();
	void DrawTargetPicker();
	void DrawInstallButtons();
	void RefreshTargets();
	const StageInstall::Target* ChosenTarget() const;

	int m_id = -1;
	FbGameFolder::Game m_exportGame = FbGameFolder::Game_BBCF;
	int m_target = 0;
	bool m_targetsLoaded = false;
	bool m_exportBusy = false;
	std::vector<StageInstall::Target> m_targets;
	std::vector<std::string> m_targetLabels;
	std::vector<bool> m_targetBackups;
	std::string m_gameFolder;
	std::string m_installStatus;
	AsyncFileDialog m_folderDialog;
	int m_slot = -1;
	bool m_focus = false;
	long m_revision = -1;
	std::vector<std::string> m_values;
	std::vector<bool> m_forced;
	std::vector<std::string> m_edited;
};
