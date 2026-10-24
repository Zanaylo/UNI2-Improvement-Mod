#pragma once

#include <d3d9.h>

namespace MagnifyRule
{
	bool WantsLinear(D3DFORMAT format, DWORD magFilter, bool scaledSource);
	bool HoldsIndices(D3DFORMAT format);
}
