#pragma once

#include "Overlay/Framework/IWindow.h"

class DummyScriptGuideWindow : public IWindow
{
public:
	DummyScriptGuideWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;
	bool GrowsToFitContent() const override { return true; }
};
