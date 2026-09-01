#pragma once

#include "Core/Hotkeys.h"
#include "Overlay/Window/ColorCustomizePanel.h"
#include "Overlay/Window/IWindow.h"
#include "Overlay/Window/PlayerCardPanel.h"

class MainWindow : public IWindow
{
public:
	MainWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void Draw() override;
	bool GrowsToFitContent() const override { return true; }
	void BeforeDraw() override;

private:
	void DrawTrainingSection();
	void DrawHitboxControls();
	void DrawHitboxTypeControls();
	void DrawFrameMeterControls();
	void DrawFrameStepControls();
	void DrawFreezeModeCombo();
	void DrawAutoPauseControls();
	void DrawStageColourControls();
	void DrawDummyScriptControls();
	void DrawScriptTab(int player);
	void DrawTimingControls();
	void DrawExtrasControls();
	void PersistAutoPause();
	void DrawConfigSection();
	void DrawConfigGeneralTab();
	void DrawKeybindsTab();
	void DrawKeyboardTab();
	void DrawReplayPatchWarning();
	void DrawReplayAccounts();
	void DrawReplaySection();
	void DrawPlayerCount();
	void DrawMusicSection();
	void DrawPerformanceSection();
	void DrawPatchSection();
	void DrawThemeSection();
	void DrawCustomSection();
	void DrawPalettesTab();
	void CaptureBind();
	void DrawFunctionBinds();
	void DrawBindRow(Hotkeys::Action action);
	void DrawBindConflicts();
	void DrawPaletteOptions();
	void DrawPaletteChoosers();
	void DrawPaletteChooser(int player);
	void StepPalette(int player, int applied, int count, int steps);

	ColorCustomizePanel m_colorCustomize;
	PlayerCardPanel m_playerCard;
};
