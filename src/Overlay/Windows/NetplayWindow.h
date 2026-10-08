#pragma once

#include "Network/NetcodeChoice.h"
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
	void DrawMeasuredPings();
	void DrawStartCapture();
	void DrawReplayUpload();
	void DrawSpectatorCatchUp();
	void DrawRoomWatch();
	void DrawNetcodeTab();
	void DrawInputDelay();
	bool DrawNetcodeOptions();
	void DrawNetcodeStatus();
	void DrawNetworkLogTab();
	void DrawRoomTab();
	void DrawOpponentsTab();
	void DrawPrivacyTab();
	void DrawRoomNamePrivacy();
	void DrawIrPrivacy();
	void DrawSpectateTab();
	void DrawSpectateHost();
	void DrawSpectateWatch();

	NetcodeChoice::Values m_netcode = {};
	bool m_netcodeRead = false;
};
