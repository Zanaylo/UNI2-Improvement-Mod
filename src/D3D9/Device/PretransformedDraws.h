#pragma once

#include <d3d9.h>

namespace PretransformedDraws
{
	using DrawPrimitive_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
	using DrawIndexedPrimitive_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE,
		INT, UINT, UINT, UINT, UINT);
	using DrawPrimitiveUP_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT,
		const void*, UINT);
	using DrawIndexedPrimitiveUP_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE,
		UINT, UINT, UINT, const void*, D3DFORMAT, const void*, UINT);

	void Attach(IDirect3DDevice9* device);

	bool Applies();
	bool IsScreenSpace();

	HRESULT DrawPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT startVertex,
		UINT primitiveCount, DrawPrimitive_t unscaled);

	HRESULT DrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertex,
		UINT minVertex, UINT vertexCount, UINT startIndex, UINT primitiveCount,
		DrawIndexedPrimitive_t unscaled);

	HRESULT DrawPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT primitiveCount,
		const void* vertices, UINT stride, DrawPrimitiveUP_t original);

	HRESULT DrawIndexedPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT minVertex,
		UINT vertexCount, UINT primitiveCount, const void* indices, D3DFORMAT indexFormat,
		const void* vertices, UINT stride, DrawIndexedPrimitiveUP_t original);

	HRESULT ReplayIndexed(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertex, UINT minVertex,
		UINT vertexCount, UINT startIndex, UINT primitiveCount, float scaleX, float scaleY,
		DrawIndexedPrimitive_t original);

	const char* GetStatusText();
}
