#include "D3D9/Device/MagnifyRule.h"

bool MagnifyRule::WantsLinear(D3DFORMAT format, DWORD magFilter, bool scaledSource)
{
	if (format == D3DFMT_UNKNOWN || magFilter != D3DTEXF_POINT || scaledSource)
		return false;

	return !HoldsIndices(format);
}

bool MagnifyRule::HoldsIndices(D3DFORMAT format)
{
	switch (format)
	{
	case D3DFMT_L8:
	case D3DFMT_A8:
	case D3DFMT_P8:
	case D3DFMT_A8P8:
	case D3DFMT_A8L8:
	case D3DFMT_L16:
		return true;

	default:
		return false;
	}
}
