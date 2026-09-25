#pragma once

#include "Overlay/Panels/MusicPanel.h"
#include "Overlay/Framework/IWindow.h"

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
