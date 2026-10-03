#pragma once

struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct tagRECT;

namespace DrawTrace
{
	void Arm();
	int LastDrawIndex();

	void OnPresentBegin(const tagRECT* sourceRect, const tagRECT* destRect);
	void OnPresentEnd();

	void OnDraw(IDirect3DDevice9* device, const char* kind, int primitiveType,
		unsigned primitiveCount, const void* vertexData = nullptr, unsigned stride = 0,
		unsigned vertexCount = 0);

	void OnClear(IDirect3DDevice9* device, unsigned long count, const void* rects, unsigned long flags,
		unsigned long colour);

	void OnSetViewport(unsigned long x, unsigned long y, unsigned long width, unsigned long height,
		unsigned long appliedWidth, unsigned long appliedHeight, long result);

	void OnStretchRect(IDirect3DSurface9* source, const tagRECT* sourceRect, IDirect3DSurface9* destination,
		const tagRECT* destinationRect);

	void OnIndexedDraw(IDirect3DDevice9* device, int primitiveType, unsigned primitiveCount,
		int baseVertex, unsigned minVertex, unsigned vertexCount);
}
