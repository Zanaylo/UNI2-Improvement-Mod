#include "D3D9/Draw/FrozenFrame.h"

#include "Core/logger.h"

namespace {

struct ScreenVertex
{
	float x;
	float y;
	float z;
	float rhw;
	float u;
	float v;
};

constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_TEX1;

IDirect3DTexture9* g_texture = nullptr;
IDirect3DStateBlock9* g_stateBlock = nullptr;
UINT g_width = 0;
UINT g_height = 0;
D3DFORMAT g_format = D3DFMT_UNKNOWN;
bool g_hasContent = false;
bool g_failed = false;

void Release()
{
	if (g_texture != nullptr)
	{
		g_texture->Release();
		g_texture = nullptr;
	}

	if (g_stateBlock != nullptr)
	{
		g_stateBlock->Release();
		g_stateBlock = nullptr;
	}

	g_hasContent = false;
}

bool EnsureTexture(IDirect3DDevice9* device)
{
	if (g_texture != nullptr)
		return true;

	if (g_failed || device == nullptr || g_width == 0 || g_height == 0 || g_format == D3DFMT_UNKNOWN)
		return false;

	const HRESULT result = device->CreateTexture(g_width, g_height, 1, D3DUSAGE_RENDERTARGET,
		g_format, D3DPOOL_DEFAULT, &g_texture, nullptr);

	if (FAILED(result))
	{
		LOG("FrozenFrame: CreateTexture failed 0x%08lx (%ux%u fmt %d)", result, g_width, g_height, (int)g_format);
		g_texture = nullptr;
		g_failed = true;
		return false;
	}

	return true;
}

bool CaptureState(IDirect3DDevice9* device)
{
	if (g_stateBlock != nullptr)
		return SUCCEEDED(g_stateBlock->Capture());

	return SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &g_stateBlock));
}

}

void FrozenFrame::OnDeviceReset(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format)
{
	Release();

	g_width = device != nullptr ? width : 0;
	g_height = device != nullptr ? height : 0;
	g_format = device != nullptr ? format : D3DFMT_UNKNOWN;
	g_failed = false;
}

void FrozenFrame::OnDeviceLost()
{
	Release();
}

void FrozenFrame::Shutdown()
{
	Release();
	g_width = 0;
	g_height = 0;
	g_format = D3DFMT_UNKNOWN;
	g_failed = false;
}

bool FrozenFrame::HoldsDeviceResources()
{
	return g_texture != nullptr || g_stateBlock != nullptr;
}

bool FrozenFrame::IsValid()
{
	return g_texture != nullptr && g_hasContent;
}

void FrozenFrame::Invalidate()
{
	g_hasContent = false;
}

void FrozenFrame::Capture(IDirect3DDevice9* device)
{
	if (!EnsureTexture(device))
		return;

	IDirect3DSurface9* backBuffer = nullptr;
	if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
		return;

	IDirect3DSurface9* destination = nullptr;
	if (SUCCEEDED(g_texture->GetSurfaceLevel(0, &destination)))
	{
		if (SUCCEEDED(device->StretchRect(backBuffer, nullptr, destination, nullptr, D3DTEXF_NONE)))
			g_hasContent = true;

		destination->Release();
	}

	backBuffer->Release();
}

bool FrozenFrame::Draw(IDirect3DDevice9* device)
{
	if (device == nullptr || !IsValid())
		return false;

	if (!CaptureState(device))
		return false;

	bool drawn = false;

	if (SUCCEEDED(device->BeginScene()))
	{
		const float width = static_cast<float>(g_width);
		const float height = static_cast<float>(g_height);

		const ScreenVertex vertices[4] = {
			{ -0.5f,          -0.5f,           0.0f, 1.0f, 0.0f, 0.0f },
			{ width - 0.5f,   -0.5f,           0.0f, 1.0f, 1.0f, 0.0f },
			{ -0.5f,          height - 0.5f,   0.0f, 1.0f, 0.0f, 1.0f },
			{ width - 0.5f,   height - 0.5f,   0.0f, 1.0f, 1.0f, 1.0f },
		};

		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);
		device->SetRenderState(D3DRS_FOGENABLE, FALSE);
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0f);

		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(kFvf);
		device->SetTexture(0, g_texture);

		drawn = SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ScreenVertex)));

		device->EndScene();
	}

	g_stateBlock->Apply();

	return drawn;
}
