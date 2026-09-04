#pragma once

#include "Overlay/Window/IWindow.h"
#include "Overlay/Window/StagesPanel.h"

#include <string>

class StagesWindow : public IWindow
{
public:
	StagesWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	StagesPanel m_panel;
};
