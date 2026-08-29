#pragma once

#include "Overlay/Window/IWindow.h"

#include <string>

class ThemeWindow : public IWindow
{
public:
	ThemeWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	void DrawThemeList(int count, int active);
	void DrawControls(int active);
	void DrawScreens(int active);
	void DrawCursorSearch();
};
