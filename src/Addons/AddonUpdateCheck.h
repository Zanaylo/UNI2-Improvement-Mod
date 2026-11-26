#pragma once

#include <string>

namespace AddonUpdateCheck
{
	void Start();
	void CheckNow();
	void Shutdown();

	void OnFrame();
	void DrawOverlay();

	bool IsChecking();
	bool IsUpdating();
	bool HasUpdates();
	int UpdateCount();
	std::string GetStatusText();

	bool IsPopupOpen();
	void OpenPopup();
	void ClosePopup();
}
