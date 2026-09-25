#pragma once

#include "Overlay/Framework/IWindow.h"
#include "Overlay/Panels/SubtitlesPanel.h"

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
