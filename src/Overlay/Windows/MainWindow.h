#pragma once

#include "Overlay/Framework/IWindow.h"

class MainWindow : public IWindow
{
public:
	MainWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void Draw() override;
	bool GrowsToFitContent() const override { return true; }
	void BeforeDraw() override;

private:
	void DrawPlayerCount();
	void DrawMusicSection();
	void DrawSoundSection();
	void DrawSubtitlesSection();
	void DrawOnlineSection();
	void DrawPerformanceSection();
	void DrawPatchSection();
	void DrawThemeSection();
	void DrawStagesSection();
	void DrawModsSection();

};
