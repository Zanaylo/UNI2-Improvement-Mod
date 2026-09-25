#pragma once

#include "Overlay/Framework/IWindow.h"

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
	void DrawNetworkLogTab();
	void DrawRoomTab();
	void DrawOpponentsTab();
	void DrawPrivacyTab();
	void DrawRoomNamePrivacy();
	void DrawSpectateTab();
	void DrawSpectateHost();
	void DrawSpectateWatch();

	char m_spectateCode[32] = {};
};
