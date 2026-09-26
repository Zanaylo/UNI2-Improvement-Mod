#pragma once

#include <Windows.h>

#include <vector>

namespace InjectedKeys
{
	struct Step
	{
		std::vector<int> keys;
		int holdFrames;
		int gapFrames;
	};

	bool Play(const std::vector<Step>& steps, DWORD timeoutMs);

	void OnFrame();

	bool IsDown(int virtualKey);
	void Merge(BYTE* keyState);
}
