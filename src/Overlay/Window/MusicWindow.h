#pragma once

#include "Overlay/Window/MusicPanel.h"
#include "Overlay/Window/IWindow.h"

#include <string>

class MusicWindow : public IWindow
{
public:
	MusicWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	MusicPanel m_panel;
};
