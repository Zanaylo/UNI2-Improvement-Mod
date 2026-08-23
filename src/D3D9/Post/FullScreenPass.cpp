#include "D3D9/Post/FullScreenPass.h"

namespace {

struct Vertex
{
	float x;
	float y;
	float z;
	float rhw;
	float u;
	float v;
};

constexpr DWORD kVertexFormat = D3DFVF_XYZRHW | D3DFVF_TEX1;

}

void FullScreenQuad::SetConstant(IDirect3DDevice9* device, unsigned slot, float x, float y,
	float z, float w)
{
	if (device == nullptr)
		return;

	const float values[4] = { x, y, z, w };
	device->SetPixelShaderConstantF(slot, values, 1);
}

void FullScreenQuad::Draw(IDirect3DDevice9* device, IDirect3DPixelShader9* shader,
	IDirect3DBaseTexture9* source, unsigned width, unsigned height, bool linear,
	IDirect3DBaseTexture9* second)
{
	if (device == nullptr || shader == nullptr || width == 0 || height == 0)
		return;

	const float right = static_cast<float>(width) - 0.5f;
	const float bottom = static_cast<float>(height) - 0.5f;

	const Vertex quad[4] = {
		{ -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
		{ right, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
		{ -0.5f, bottom, 0.0f, 1.0f, 0.0f, 1.0f },
		{ right, bottom, 0.0f, 1.0f, 1.0f, 1.0f },
	};

	const DWORD filter = linear ? D3DTEXF_LINEAR : D3DTEXF_POINT;

	device->SetVertexShader(nullptr);
	device->SetPixelShader(shader);
	device->SetFVF(kVertexFormat);
	device->SetTexture(0, source);
	device->SetTexture(1, second);

	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0f);
	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

	device->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
	device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

	device->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	device->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device->SetSamplerState(1, D3DSAMP_SRGBTEXTURE, FALSE);

	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));
}

PixelShaderHandle::PixelShaderHandle()
	: m_shader(nullptr)
	, m_bytecode(nullptr)
{
}

PixelShaderHandle::~PixelShaderHandle()
{
	Release();
}

bool PixelShaderHandle::Ensure(IDirect3DDevice9* device, const void* bytecode)
{
	if (device == nullptr || bytecode == nullptr)
		return false;

	if (m_shader != nullptr && m_bytecode == bytecode)
		return true;

	Release();

	if (FAILED(device->CreatePixelShader(static_cast<const DWORD*>(bytecode), &m_shader)))
	{
		m_shader = nullptr;
		return false;
	}

	m_bytecode = bytecode;
	return true;
}

void PixelShaderHandle::Release()
{
	if (m_shader != nullptr)
		m_shader->Release();

	m_shader = nullptr;
	m_bytecode = nullptr;
}
