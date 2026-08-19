#pragma once

#include "Overlay/Window/IWindow.h"

class HitboxLegendWindow : public IWindow
{
public:
	HitboxLegendWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;
	bool GrowsToFitContent() const override { return true; }
};
