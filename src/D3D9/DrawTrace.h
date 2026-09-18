#pragma once

struct IDirect3DDevice9;
struct tagRECT;

namespace DrawTrace
{
	void Arm();

	void OnPresentBegin(const tagRECT* sourceRect, const tagRECT* destRect);
	void OnPresentEnd();

	void OnDraw(IDirect3DDevice9* device, const char* kind, int primitiveType,
		unsigned primitiveCount, const void* vertexData = nullptr, unsigned stride = 0,
		unsigned vertexCount = 0);

	void OnIndexedDraw(IDirect3DDevice9* device, int primitiveType, unsigned primitiveCount,
		int baseVertex, unsigned minVertex, unsigned vertexCount);
}
