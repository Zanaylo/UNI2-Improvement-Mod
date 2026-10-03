#pragma once

#include "D3D9/Device/PretransformedDraws.h"

#include <d3d9.h>

namespace StageDetail
{
	struct IndexedDraw
	{
		D3DPRIMITIVETYPE type;
		INT baseVertex;
		UINT minVertex;
		UINT vertexCount;
		UINT startIndex;
		UINT primitiveCount;
	};

	void OnIndexedDraw(IDirect3DDevice9* device, const IndexedDraw& draw,
		PretransformedDraws::DrawIndexedPrimitive_t original);

	bool SmoothReducedComposite(IDirect3DDevice9* device);
	void EndSmoothing(IDirect3DDevice9* device);

	bool IsReady();
	IDirect3DTexture9* Detailed();
	IDirect3DTexture9* Snapshot();

	void OnPresent();
	void OnDeviceLost();
	bool HoldsDeviceResources();
}
