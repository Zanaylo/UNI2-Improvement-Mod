#include "Game/Stages/StageCapture.h"

#include "Core/logger.h"

#include <d3d9.h>

#include <cstring>

namespace {

constexpr UINT kFeedWidth = 640;
constexpr UINT kFeedHeight = 360;
constexpr LONG kBoundFrames = 2;

IDirect3DTexture9* g_card = nullptr;
IDirect3DTexture9* g_feed = nullptr;
IDirect3DSurface9* g_feedSurface = nullptr;
LONG g_lastBound = -kBoundFrames - 1;
LONG g_frame = 0;
bool g_failed = false;

bool Stamped(const void* source, UINT bytes)
{
	const size_t length = strlen(StageCapture::kStamp);

	if (source == nullptr || bytes < StageCapture::kStampAt + length)
		return false;

	return memcmp(static_cast<const char*>(source) + StageCapture::kStampAt, StageCapture::kStamp,
		length) == 0;
}

void ReleaseFeed()
{
	if (g_feedSurface != nullptr)
		g_feedSurface->Release();

	if (g_feed != nullptr)
		g_feed->Release();

	g_feedSurface = nullptr;
	g_feed = nullptr;
}

bool MakeFeed(IDirect3DDevice9* device)
{
	if (g_feedSurface != nullptr)
		return true;

	if (g_failed)
		return false;

	const HRESULT made = device->CreateTexture(kFeedWidth, kFeedHeight, 1, D3DUSAGE_RENDERTARGET,
		D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &g_feed, nullptr);

	if (SUCCEEDED(made) && SUCCEEDED(g_feed->GetSurfaceLevel(0, &g_feedSurface)))
		return true;

	ReleaseFeed();
	g_failed = true;
	LOG("StageCapture: the live screen texture could not be made (0x%08lx), the monitors keep "
		"their card", static_cast<unsigned long>(made));

	return false;
}

}

void StageCapture::Adopt(const void* source, unsigned int bytes, IDirect3DTexture9* texture)
{
	if (texture == nullptr || !Stamped(source, bytes))
		return;

	texture->AddRef();

	if (g_card != nullptr)
		g_card->Release();

	g_card = texture;
	g_failed = false;

	LOG("StageCapture: this stage shows the live screen on its monitors");
}

IDirect3DBaseTexture9* StageCapture::OnSetTexture(IDirect3DBaseTexture9* texture)
{
	if (texture == nullptr || texture != g_card)
		return texture;

	g_lastBound = g_frame;

	return g_feed != nullptr ? g_feed : texture;
}

void StageCapture::OnPresent(IDirect3DDevice9* device)
{
	++g_frame;

	if (g_card == nullptr || g_frame - g_lastBound > kBoundFrames || !MakeFeed(device))
		return;

	IDirect3DSurface9* back = nullptr;

	if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)))
		return;

	device->StretchRect(back, nullptr, g_feedSurface, nullptr, D3DTEXF_LINEAR);
	back->Release();
}

void StageCapture::OnDeviceLost()
{
	ReleaseFeed();
}
