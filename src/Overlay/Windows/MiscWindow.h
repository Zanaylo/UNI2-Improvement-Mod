#pragma once

#include "Overlay/Framework/IWindow.h"
#include "Overlay/Panels/MiscPanel.h"

#include <string>

class MiscWindow : public IWindow
{
public:
	MiscWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	MiscPanel m_panel;
};
