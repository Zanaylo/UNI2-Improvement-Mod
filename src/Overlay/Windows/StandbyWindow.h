#pragma once

#include "Overlay/Framework/IWindow.h"

#include <string>

class StandbyWindow : public IWindow
{
public:
	StandbyWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	void DrawReason() const;
	void DrawUpdateState() const;
};
