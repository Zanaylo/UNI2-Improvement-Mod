#pragma once

#include "Overlay/Window/IWindow.h"
#include "Overlay/Window/ModsPanel.h"

#include <string>

class ModsWindow : public IWindow
{
public:
	ModsWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	ModsPanel m_panel;
};
