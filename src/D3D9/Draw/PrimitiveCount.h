#pragma once

#include <d3d9.h>

inline unsigned PrimitiveVertexCount(int primitiveType, unsigned primitiveCount)
{
	switch (primitiveType)
	{
	case D3DPT_POINTLIST:
		return primitiveCount;
	case D3DPT_LINELIST:
		return primitiveCount * 2;
	case D3DPT_LINESTRIP:
		return primitiveCount + 1;
	case D3DPT_TRIANGLELIST:
		return primitiveCount * 3;
	default:
		return primitiveCount + 2;
	}
}
