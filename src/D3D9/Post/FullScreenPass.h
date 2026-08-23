#pragma once

#include <d3d9.h>

namespace FullScreenQuad
{
	void SetConstant(IDirect3DDevice9* device, unsigned slot, float x, float y, float z, float w);

	void Draw(IDirect3DDevice9* device, IDirect3DPixelShader9* shader,
		IDirect3DBaseTexture9* source, unsigned width, unsigned height, bool linear,
		IDirect3DBaseTexture9* second = nullptr);
}

class PixelShaderHandle
{
public:
	PixelShaderHandle();
	~PixelShaderHandle();

	PixelShaderHandle(const PixelShaderHandle&) = delete;
	PixelShaderHandle& operator=(const PixelShaderHandle&) = delete;

	bool Ensure(IDirect3DDevice9* device, const void* bytecode);
	void Release();

	IDirect3DPixelShader9* Get() const { return m_shader; }

private:
	IDirect3DPixelShader9* m_shader;
	const void* m_bytecode;
};
