#pragma once

#include "Overlay/Window/IWindow.h"
#include "Overlay/Window/SubtitlesPanel.h"

#include <string>

class SubtitlesWindow : public IWindow
{
public:
	SubtitlesWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	SubtitlesPanel m_panel;
};
