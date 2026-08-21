#pragma once

#include "Overlay/Window/IWindow.h"

class PlayerControlWindow : public IWindow
{
public:
	PlayerControlWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;
	void AfterDraw() override;
	bool GrowsToFitContent() const override { return true; }

private:
	void DrawSide(int player);
	void DrawKeyboard();
	void DrawButtonCalibration();
};
