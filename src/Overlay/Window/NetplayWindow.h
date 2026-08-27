#pragma once

#include "Overlay/Window/IWindow.h"

class NetplayWindow : public IWindow
{
public:
	NetplayWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	void DrawRollbackTab();
	void DrawStartCapture();
	void DrawRoomTab();
	void DrawOpponentsTab();
};
