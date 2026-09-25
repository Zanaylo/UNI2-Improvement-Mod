#pragma once

#include <Windows.h>

struct IDirect3D9;

namespace D3D9Proxy
{
	bool IsActive();
	HMODULE RealModule();
	const char* LoadedAs();
}
