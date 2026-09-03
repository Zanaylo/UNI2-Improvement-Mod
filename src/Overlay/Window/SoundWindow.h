#pragma once

#include "Core/AsyncFileDialog.h"
#include "Overlay/Window/IWindow.h"

#include <string>
#include <vector>

class SoundWindow : public IWindow
{
public:
	SoundWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	void DrawReplace();
	void DrawPacks();
	void DrawHowTo();

	void DrawPicker();
	void DrawGetVoice();
	void DrawList();
	void DrawRow(int index);
	void DrawNewPack();

	void UpdateVisible();
	void Reload();
	void AskForPack(int index);

	void DrawChooser(const char* label);
	void PumpDialogs();

	bool Wanted(int index) const;

	AsyncFileDialog m_import;
	AsyncFileDialog m_export;
	AsyncFileDialog m_replace;
	AsyncFileDialog m_uni;

	std::vector<int> m_visible;

	std::string m_exporting;
	int m_replacing = -1;
	int m_wanted = -1;
	int m_chara = 0;
	int m_visibleFor = -1;
	int m_visibleChara = -1;
	bool m_importing = false;
	bool m_askPack = false;
	long m_revision = -1;
	char m_packName[64] = {};
	char m_visibleFilter[64] = {};
	char m_filter[64] = {};
	char m_status[192] = {};
};
