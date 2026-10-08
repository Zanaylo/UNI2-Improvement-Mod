#pragma once

namespace DisplaySync
{
	void OnFrame();
	void OnPresenting();
	void OnPresented();

	const char* GetStatusText();
}
