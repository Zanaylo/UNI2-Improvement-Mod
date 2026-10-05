#include "D3D9/Device/StageDetail.h"

#include "Core/logger.h"
#include "D3D9/Device/DeviceHooks.h"
#include "D3D9/Device/ScaledTargets.h"
#include "D3D9/Post/ScratchTarget.h"
#include "Game/Display/InternalResolution.h"

namespace {

constexpr D3DCOLOR kOpaqueBlack = 0xff000000;

ScratchTarget g_detailed;
ScratchTarget g_snapshot;
bool g_ready = false;
bool g_stageResolved = false;
bool g_announced = false;

class BoundTarget
{
public:
	explicit BoundTarget(IDirect3DDevice9* device)
	{
		if (FAILED(device->GetRenderTarget(0, &m_surface)))
			m_surface = nullptr;
	}

	~BoundTarget()
	{
		if (m_surface != nullptr)
			m_surface->Release();
	}

	BoundTarget(const BoundTarget&) = delete;
	BoundTarget& operator=(const BoundTarget&) = delete;

	IDirect3DSurface9* Get() const { return m_surface; }

private:
	IDirect3DSurface9* m_surface = nullptr;
};

bool SamplesScaledTarget(IDirect3DDevice9* device)
{
	IDirect3DBaseTexture9* texture = nullptr;
	if (FAILED(device->GetTexture(0, &texture)) || texture == nullptr)
		return false;

	const bool scaled = ScaledTargets::IsScaledTexture(texture);
	texture->Release();
	return scaled;
}

bool IsSceneLayer(IDirect3DSurface9* surface, D3DSURFACE_DESC& outDesc)
{
	return surface != nullptr && SUCCEEDED(surface->GetDesc(&outDesc)) &&
		outDesc.Width == InternalResolution::kBaseWidth && outDesc.Height == InternalResolution::kBaseHeight;
}

bool IsStageComposite(IDirect3DDevice9* device)
{
	return g_stageResolved && ScaledTargets::IsActive() && PretransformedDraws::IsScreenSpace() &&
		!ScaledTargets::BoundIsScaled() && SamplesScaledTarget(device);
}

bool AddsDetail(IDirect3DDevice9* device)
{
	return !g_ready && !ScaledTargets::IsReduced() && IsStageComposite(device);
}

bool OutputSize(unsigned& outWidth, unsigned& outHeight)
{
	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	outWidth = present.BackBufferWidth;
	outHeight = present.BackBufferHeight;

	return outWidth > InternalResolution::kBaseWidth && outHeight > InternalResolution::kBaseHeight;
}

void Replay(IDirect3DDevice9* device, IDirect3DSurface9* sceneLayer, const StageDetail::IndexedDraw& draw,
	PretransformedDraws::DrawIndexedPrimitive_t original)
{
	D3DVIEWPORT9 viewport = {};
	device->GetViewport(&viewport);

	device->SetRenderTarget(0, g_detailed.Surface());
	device->Clear(0, nullptr, D3DCLEAR_TARGET, kOpaqueBlack, 1.0f, 0);

	const float scaleX = static_cast<float>(g_detailed.Width()) / static_cast<float>(InternalResolution::kBaseWidth);
	const float scaleY = static_cast<float>(g_detailed.Height()) / static_cast<float>(InternalResolution::kBaseHeight);

	const HRESULT result = PretransformedDraws::ReplayIndexed(device, draw.type, draw.baseVertex, draw.minVertex,
		draw.vertexCount, draw.startIndex, draw.primitiveCount, scaleX, scaleY, original);

	device->SetRenderTarget(0, sceneLayer);
	device->SetViewport(&viewport);

	g_ready = SUCCEEDED(result);

	if (g_announced)
		return;

	g_announced = true;
	LOG("[InternalResolution] stage detail: the stage is composited again at %ux%u (0x%08lx)", g_detailed.Width(),
		g_detailed.Height(), static_cast<unsigned long>(result));
}

}

void StageDetail::OnIndexedDraw(IDirect3DDevice9* device, const IndexedDraw& draw,
	PretransformedDraws::DrawIndexedPrimitive_t original)
{
	if (device == nullptr || !AddsDetail(device))
		return;

	unsigned width = 0;
	unsigned height = 0;
	if (!OutputSize(width, height))
		return;

	BoundTarget sceneLayer(device);
	D3DSURFACE_DESC desc = {};

	if (!IsSceneLayer(sceneLayer.Get(), desc) || !g_snapshot.Ensure(device, desc.Width, desc.Height, desc.Format) ||
		!g_detailed.Ensure(device, width, height, desc.Format))
	{
		return;
	}

	if (FAILED(device->StretchRect(sceneLayer.Get(), nullptr, g_snapshot.Surface(), nullptr, D3DTEXF_NONE)))
		return;

	Replay(device, sceneLayer.Get(), draw, original);
}

bool StageDetail::SmoothReducedComposite(IDirect3DDevice9* device)
{
	if (device == nullptr || !ScaledTargets::IsReduced() || !IsStageComposite(device))
		return false;

	DWORD filter = 0;
	if (FAILED(device->GetSamplerState(0, D3DSAMP_MAGFILTER, &filter)) || filter != D3DTEXF_POINT)
		return false;

	return SUCCEEDED(device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
}

void StageDetail::EndSmoothing(IDirect3DDevice9* device)
{
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
}

bool StageDetail::IsReady()
{
	return g_ready;
}

IDirect3DTexture9* StageDetail::Detailed()
{
	return g_detailed.Texture();
}

IDirect3DTexture9* StageDetail::Snapshot()
{
	return g_snapshot.Texture();
}

void StageDetail::OnStageResolved()
{
	g_stageResolved = true;
}

void StageDetail::OnPresent()
{
	g_ready = false;
	g_stageResolved = false;
}

void StageDetail::OnDeviceLost()
{
	g_ready = false;
	g_stageResolved = false;
	g_detailed.Release();
	g_snapshot.Release();
}

bool StageDetail::HoldsDeviceResources()
{
	return g_detailed.IsHeld() || g_snapshot.IsHeld();
}
