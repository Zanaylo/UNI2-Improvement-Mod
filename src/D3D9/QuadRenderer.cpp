#include "D3D9/QuadRenderer.h"

#include <vector>

namespace {

struct Vertex
{
	float x;
	float y;
	float z;
	float rhw;
	uint32_t color;
	float u;
	float v;
};

constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

constexpr int kVerticesPerQuad = 6;
constexpr size_t kInitialCapacity = 2048 * kVerticesPerQuad;

IDirect3DDevice9* g_device = nullptr;
IDirect3DStateBlock9* g_stateBlock = nullptr;
IDirect3DTexture9* g_white = nullptr;
bool g_openedScene = false;

std::vector<Vertex> g_batch;
IDirect3DTexture9* g_batchTexture = nullptr;

bool EnsureWhiteTexture(IDirect3DDevice9* device)
{
	if (g_white != nullptr)
		return true;

	if (FAILED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_white, nullptr)))
		return false;

	D3DLOCKED_RECT locked = {};
	if (FAILED(g_white->LockRect(0, &locked, nullptr, 0)))
	{
		g_white->Release();
		g_white = nullptr;
		return false;
	}

	*static_cast<uint32_t*>(locked.pBits) = 0xFFFFFFFF;
	g_white->UnlockRect(0);
	return true;
}

void Flush()
{
	if (g_device == nullptr || g_batch.empty())
		return;

	g_device->SetTexture(0, g_batchTexture);
	g_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(g_batch.size() / 3),
		g_batch.data(), sizeof(Vertex));

	g_batch.clear();
}

void PushQuadShaded(IDirect3DTexture9* texture, float x, float y, float width, float height,
	float u0, float v0, float u1, float v1, uint32_t topColor, uint32_t bottomColor)
{
	if (g_device == nullptr || width <= 0.0f || height <= 0.0f)
		return;

	if (texture == nullptr)
		texture = g_white;

	if (texture != g_batchTexture)
	{
		Flush();
		g_batchTexture = texture;
	}

	const float left = x - 0.5f;
	const float top = y - 0.5f;
	const float right = left + width;
	const float bottom = top + height;

	const Vertex topLeft     = { left,  top,    0.0f, 1.0f, topColor,    u0, v0 };
	const Vertex topRight    = { right, top,    0.0f, 1.0f, topColor,    u1, v0 };
	const Vertex bottomLeft  = { left,  bottom, 0.0f, 1.0f, bottomColor, u0, v1 };
	const Vertex bottomRight = { right, bottom, 0.0f, 1.0f, bottomColor, u1, v1 };

	g_batch.push_back(topLeft);
	g_batch.push_back(topRight);
	g_batch.push_back(bottomLeft);
	g_batch.push_back(topRight);
	g_batch.push_back(bottomRight);
	g_batch.push_back(bottomLeft);
}

void PushQuad(IDirect3DTexture9* texture, float x, float y, float width, float height,
	float u0, float v0, float u1, float v1, uint32_t color)
{
	PushQuadShaded(texture, x, y, width, height, u0, v0, u1, v1, color, color);
}

}

bool QuadRenderer::Begin(IDirect3DDevice9* device)
{
	if (device == nullptr || g_device != nullptr)
		return false;

	if (!EnsureWhiteTexture(device))
		return false;

	if (g_stateBlock == nullptr)
	{
		if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &g_stateBlock)))
			return false;
	}
	else if (FAILED(g_stateBlock->Capture()))
	{
		return false;
	}

	g_openedScene = SUCCEEDED(device->BeginScene());
	if (!g_openedScene)
		return false;

	g_device = device;

	if (g_batch.capacity() < kInitialCapacity)
		g_batch.reserve(kInitialCapacity);

	g_batch.clear();
	g_batchTexture = g_white;

	device->SetVertexShader(nullptr);
	device->SetPixelShader(nullptr);
	device->SetFVF(kFvf);

	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0f);
	device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);

	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	return true;
}

void QuadRenderer::End()
{
	if (g_device == nullptr)
		return;

	Flush();

	g_device->SetTexture(0, nullptr);

	if (g_openedScene)
	{
		g_device->EndScene();
		g_openedScene = false;
	}

	if (g_stateBlock != nullptr)
		g_stateBlock->Apply();

	g_device = nullptr;
}

void QuadRenderer::OnDeviceLost()
{
	if (g_stateBlock != nullptr)
	{
		g_stateBlock->Release();
		g_stateBlock = nullptr;
	}

	if (g_white != nullptr)
	{
		g_white->Release();
		g_white = nullptr;
	}

	g_batch.clear();
	g_batchTexture = nullptr;
}

void QuadRenderer::FillRect(float x, float y, float width, float height, uint32_t color)
{
	PushQuad(nullptr, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

void QuadRenderer::FillRectVertical(float x, float y, float width, float height, uint32_t top,
	uint32_t bottom)
{
	PushQuadShaded(nullptr, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, top, bottom);
}

void QuadRenderer::StrokeRect(float x, float y, float width, float height, float thickness,
	uint32_t color)
{
	if (thickness <= 0.0f || width <= 0.0f || height <= 0.0f)
		return;

	FillRect(x, y, width, thickness, color);
	FillRect(x, y + height - thickness, width, thickness, color);
	FillRect(x, y + thickness, thickness, height - thickness * 2.0f, color);
	FillRect(x + width - thickness, y + thickness, thickness, height - thickness * 2.0f, color);
}

void QuadRenderer::TexturedRect(IDirect3DTexture9* texture, float x, float y, float width,
	float height, float u0, float v0, float u1, float v1, uint32_t tint)
{
	PushQuad(texture, x, y, width, height, u0, v0, u1, v1, tint);
}

void QuadRenderer::NineSlice(IDirect3DTexture9* texture, unsigned textureWidth,
	unsigned textureHeight, float x, float y, float width, float height,
	int srcX, int srcY, int srcWidth, int srcHeight,
	int left, int top, int right, int bottom, uint32_t tint)
{
	if (texture == nullptr || textureWidth == 0 || textureHeight == 0)
		return;

	const float su[4] =
	{
		static_cast<float>(srcX),
		static_cast<float>(srcX + left),
		static_cast<float>(srcX + srcWidth - right),
		static_cast<float>(srcX + srcWidth)
	};
	const float sv[4] =
	{
		static_cast<float>(srcY),
		static_cast<float>(srcY + top),
		static_cast<float>(srcY + srcHeight - bottom),
		static_cast<float>(srcY + srcHeight)
	};

	const float du[4] = { x, x + left, x + width - right, x + width };
	const float dv[4] = { y, y + top, y + height - bottom, y + height };

	const float tw = static_cast<float>(textureWidth);
	const float th = static_cast<float>(textureHeight);

	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 3; ++col)
		{
			const float dw = du[col + 1] - du[col];
			const float dh = dv[row + 1] - dv[row];
			if (dw <= 0.0f || dh <= 0.0f)
				continue;

			PushQuad(texture, du[col], dv[row], dw, dh,
				su[col] / tw, sv[row] / th, su[col + 1] / tw, sv[row + 1] / th, tint);
		}
	}
}
