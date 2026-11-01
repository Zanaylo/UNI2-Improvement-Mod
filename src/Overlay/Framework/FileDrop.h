#pragma once

#include <Windows.h>

#include <string>
#include <vector>

namespace FileDrop
{
	void Attach(HWND window);
	void Accept(bool accept);

	bool Handle(UINT message, WPARAM wParam);
	bool Take(std::vector<std::string>& out);
}
