#include "D3D9/Device/ScaledDeviceState.h"

#include "Core/logger.h"
#include "D3D9/Device/GameCaller.h"
#include "D3D9/Device/ScaledTargets.h"
#include "D3D9/Draw/DrawTrace.h"
#include "Hooks/GameHook.h"

namespace {

constexpr int kUpdateSurfaceIndex = 30;
constexpr int kGetRenderTargetDataIndex = 32;
constexpr int kStretchRectIndex = 34;
constexpr int kColorFillIndex = 35;
constexpr int kSetViewportIndex = 47;
constexpr int kGetViewportIndex = 48;
constexpr int kSetScissorRectIndex = 75;
constexpr int kGetScissorRectIndex = 76;

constexpr DWORD kMaxClearRects = 16;

using UpdateSurface_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*,
	const RECT*, IDirect3DSurface9*, const POINT*);
using GetRenderTargetData_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*,
	IDirect3DSurface9*);
using StretchRect_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*,
	const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);
using ColorFill_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*,
	D3DCOLOR);
using SetViewport_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const D3DVIEWPORT9*);
using GetViewport_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DVIEWPORT9*);
using SetScissorRect_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*);
using GetScissorRect_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, RECT*);

GameHook<UpdateSurface_t> g_updateSurfaceHook("IDirect3DDevice9::UpdateSurface");
GameHook<GetRenderTargetData_t> g_getRenderTargetDataHook("IDirect3DDevice9::GetRenderTargetData");
GameHook<StretchRect_t> g_stretchRectHook("IDirect3DDevice9::StretchRect");
GameHook<ColorFill_t> g_colorFillHook("IDirect3DDevice9::ColorFill");
GameHook<SetViewport_t> g_setViewportHook("IDirect3DDevice9::SetViewport");
GameHook<GetViewport_t> g_getViewportHook("IDirect3DDevice9::GetViewport");
GameHook<SetScissorRect_t> g_setScissorRectHook("IDirect3DDevice9::SetScissorRect");
GameHook<GetScissorRect_t> g_getScissorRectHook("IDirect3DDevice9::GetScissorRect");

D3DRECT g_clearRects[kMaxClearRects] = {};

bool g_reportedReadback = false;
bool g_reportedUpdate = false;

RECT ToScaled(const RECT& rect)
{
	return { ScaledTargets::ToScaledX(rect.left), ScaledTargets::ToScaledY(rect.top),
		ScaledTargets::ToScaledX(rect.right), ScaledTargets::ToScaledY(rect.bottom) };
}

RECT ToBase(const RECT& rect)
{
	return { ScaledTargets::ToBaseX(rect.left), ScaledTargets::ToBaseY(rect.top),
		ScaledTargets::ToBaseX(rect.right), ScaledTargets::ToBaseY(rect.bottom) };
}

DWORD ToScaledX(DWORD value)
{
	return static_cast<DWORD>(ScaledTargets::ToScaledX(static_cast<LONG>(value)));
}

DWORD ToScaledY(DWORD value)
{
	return static_cast<DWORD>(ScaledTargets::ToScaledY(static_cast<LONG>(value)));
}

DWORD ToBaseX(DWORD value)
{
	return static_cast<DWORD>(ScaledTargets::ToBaseX(static_cast<LONG>(value)));
}

DWORD ToBaseY(DWORD value)
{
	return static_cast<DWORD>(ScaledTargets::ToBaseY(static_cast<LONG>(value)));
}

const RECT* RectFor(IDirect3DSurface9* surface, const RECT* rect, RECT& storage)
{
	if (rect == nullptr || !ScaledTargets::IsScaled(surface))
		return rect;

	storage = ToScaled(*rect);
	return &storage;
}

HRESULT STDMETHODCALLTYPE HookedStretchRect(IDirect3DDevice9* device, IDirect3DSurface9* source,
	const RECT* sourceRect, IDirect3DSurface9* destination, const RECT* destinationRect,
	D3DTEXTUREFILTERTYPE filter)
{
	if (!IS_GAME_CALLER())
		return g_stretchRectHook.Original()(device, source, sourceRect, destination, destinationRect, filter);

	RECT sourceStorage = {};
	RECT destinationStorage = {};
	const RECT* const scaledSource = RectFor(source, sourceRect, sourceStorage);
	const RECT* const scaledDestination = RectFor(destination, destinationRect, destinationStorage);

	DrawTrace::OnStretchRect(source, scaledSource, destination, scaledDestination);

	return g_stretchRectHook.Original()(device, source, scaledSource, destination, scaledDestination, filter);
}

HRESULT STDMETHODCALLTYPE HookedColorFill(IDirect3DDevice9* device, IDirect3DSurface9* surface,
	const RECT* rect, D3DCOLOR color)
{
	if (!IS_GAME_CALLER())
		return g_colorFillHook.Original()(device, surface, rect, color);

	RECT storage = {};
	return g_colorFillHook.Original()(device, surface, RectFor(surface, rect, storage), color);
}

HRESULT STDMETHODCALLTYPE HookedGetRenderTargetData(IDirect3DDevice9* device,
	IDirect3DSurface9* renderTarget, IDirect3DSurface9* destination)
{
	const HRESULT result = g_getRenderTargetDataHook.Original()(device, renderTarget, destination);

	if (g_reportedReadback || !ScaledTargets::IsScaled(renderTarget))
		return result;

	g_reportedReadback = true;
	LOG("[InternalResolution] the game read back a scaled target (0x%08lx)", static_cast<unsigned long>(result));
	return result;
}

HRESULT STDMETHODCALLTYPE HookedUpdateSurface(IDirect3DDevice9* device, IDirect3DSurface9* source,
	const RECT* sourceRect, IDirect3DSurface9* destination, const POINT* destinationPoint)
{
	const HRESULT result = g_updateSurfaceHook.Original()(device, source, sourceRect, destination,
		destinationPoint);

	if (g_reportedUpdate || !ScaledTargets::IsScaled(destination))
		return result;

	g_reportedUpdate = true;
	LOG("[InternalResolution] the game updated a scaled surface from memory (0x%08lx)",
		static_cast<unsigned long>(result));
	return result;
}

HRESULT STDMETHODCALLTYPE HookedSetViewport(IDirect3DDevice9* device, const D3DVIEWPORT9* viewport)
{
	if (viewport == nullptr || !ScaledTargets::BoundIsScaled() || !IS_GAME_CALLER())
	{
		const HRESULT result = g_setViewportHook.Original()(device, viewport);

		if (viewport != nullptr)
		{
			DrawTrace::OnSetViewport(viewport->X, viewport->Y, viewport->Width, viewport->Height, viewport->Width,
				viewport->Height, result);
		}

		return result;
	}

	D3DVIEWPORT9 scaled = *viewport;
	scaled.X = ToScaledX(viewport->X);
	scaled.Y = ToScaledY(viewport->Y);
	scaled.Width = ToScaledX(viewport->Width);
	scaled.Height = ToScaledY(viewport->Height);

	const HRESULT result = g_setViewportHook.Original()(device, &scaled);
	DrawTrace::OnSetViewport(viewport->X, viewport->Y, viewport->Width, viewport->Height, scaled.Width,
		scaled.Height, result);
	return result;
}

HRESULT STDMETHODCALLTYPE HookedGetViewport(IDirect3DDevice9* device, D3DVIEWPORT9* viewport)
{
	const HRESULT result = g_getViewportHook.Original()(device, viewport);

	if (FAILED(result) || viewport == nullptr || !ScaledTargets::BoundIsScaled() || !IS_GAME_CALLER())
		return result;

	viewport->X = ToBaseX(viewport->X);
	viewport->Y = ToBaseY(viewport->Y);
	viewport->Width = ToBaseX(viewport->Width);
	viewport->Height = ToBaseY(viewport->Height);
	return result;
}

HRESULT STDMETHODCALLTYPE HookedSetScissorRect(IDirect3DDevice9* device, const RECT* rect)
{
	if (rect == nullptr || !ScaledTargets::BoundIsScaled() || !IS_GAME_CALLER())
		return g_setScissorRectHook.Original()(device, rect);

	const RECT scaled = ToScaled(*rect);
	return g_setScissorRectHook.Original()(device, &scaled);
}

HRESULT STDMETHODCALLTYPE HookedGetScissorRect(IDirect3DDevice9* device, RECT* rect)
{
	const HRESULT result = g_getScissorRectHook.Original()(device, rect);

	if (SUCCEEDED(result) && rect != nullptr && ScaledTargets::BoundIsScaled() && IS_GAME_CALLER())
		*rect = ToBase(*rect);

	return result;
}

template <typename Fn, typename Handler>
void Hook(void** vtable, int index, GameHook<Fn>& hook, Handler handler)
{
	if (hook.IsLive() || hook.Install(vtable[index], handler))
		return;

	LOG("[InternalResolution] could not hook %s", hook.Label());
}

}

void ScaledDeviceState::Attach(IDirect3DDevice9* device)
{
	void** const vtable = *reinterpret_cast<void***>(device);

	Hook(vtable, kUpdateSurfaceIndex, g_updateSurfaceHook, &HookedUpdateSurface);
	Hook(vtable, kGetRenderTargetDataIndex, g_getRenderTargetDataHook, &HookedGetRenderTargetData);
	Hook(vtable, kStretchRectIndex, g_stretchRectHook, &HookedStretchRect);
	Hook(vtable, kColorFillIndex, g_colorFillHook, &HookedColorFill);
	Hook(vtable, kSetViewportIndex, g_setViewportHook, &HookedSetViewport);
	Hook(vtable, kGetViewportIndex, g_getViewportHook, &HookedGetViewport);
	Hook(vtable, kSetScissorRectIndex, g_setScissorRectHook, &HookedSetScissorRect);
	Hook(vtable, kGetScissorRectIndex, g_getScissorRectHook, &HookedGetScissorRect);
}

const D3DRECT* ScaledDeviceState::ScaleClearRects(DWORD count, const D3DRECT* rects)
{
	if (!ScaledTargets::BoundIsScaled() || rects == nullptr || count == 0 || count > kMaxClearRects)
		return rects;

	for (DWORD i = 0; i < count; ++i)
	{
		g_clearRects[i].x1 = ScaledTargets::ToScaledX(rects[i].x1);
		g_clearRects[i].y1 = ScaledTargets::ToScaledY(rects[i].y1);
		g_clearRects[i].x2 = ScaledTargets::ToScaledX(rects[i].x2);
		g_clearRects[i].y2 = ScaledTargets::ToScaledY(rects[i].y2);
	}

	return g_clearRects;
}
