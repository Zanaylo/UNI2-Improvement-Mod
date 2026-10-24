#pragma once

#include <d3d9.h>

namespace ExclusivePlan
{
	struct Size
	{
		UINT width;
		UINT height;
	};

	bool Wants(const D3DPRESENT_PARAMETERS& display, Size wanted);
	Size DisplayFor(const D3DPRESENT_PARAMETERS& asked, Size drawn, Size display);
}
