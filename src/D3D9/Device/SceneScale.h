#pragma once

namespace SceneScale
{
	bool Install();
	void Apply();
	void OnFrame();

	bool IsApplied();
	bool GetSize(int& outWidth, int& outHeight);

	const char* GetStatusText();
}
