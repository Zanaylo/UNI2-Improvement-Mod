#pragma once

#include <d3d9.h>

#include <string>

namespace FrameGrab
{
	bool Capture(const std::string& path, DWORD timeoutMs, std::string& outReply);

	void OnPresent(IDirect3DDevice9* device);
}
