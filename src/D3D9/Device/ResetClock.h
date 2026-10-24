#pragma once

namespace ResetClock
{
	void OnFocus(bool active);
	void OnResetBegan();
	void OnModReleased();
	void OnResetReturned(bool succeeded);
	void OnPresented();
}
