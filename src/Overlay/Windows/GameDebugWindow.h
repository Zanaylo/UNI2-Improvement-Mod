#pragma once

#include "Overlay/Framework/IWindow.h"

#include <string>

class GameDebugWindow : public IWindow
{
public:
	GameDebugWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	void DrawOptions(bool changesTheGame);
	void DrawStatus();
};
