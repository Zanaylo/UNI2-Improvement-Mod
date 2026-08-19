#pragma once

#include "Overlay/Window/IWindow.h"

class PerformanceWindow : public IWindow
{
public:
	PerformanceWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	void DrawPerformanceTab();
	void DrawPotatoTab();
	void DrawMetricsTab();

	void DrawWhatIsHappening();
	bool DrawOptions();
	bool DrawDisplayGroup();
	bool DrawAdvanced();
	bool DrawPresets();
};
