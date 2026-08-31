#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/GamePatches.h"
#include "Overlay/Window/IWindow.h"

#include <string>

class PatchWindow : public IWindow
{
public:
	PatchWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	void DrawActive();
	void DrawRestart();
	void DrawCoverage(const GamePatches::Patch& patch);
	void DrawEngine(const GamePatches::Patch* patch);
	void DrawTable();
	void DrawInstalledRow();
	void DrawRow(int index);
	void DrawInstall();

	AsyncFileDialog m_folderDialog;
	char m_name[64] = {};
	char m_status[256] = {};
};
