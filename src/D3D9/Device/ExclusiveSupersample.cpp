#include "D3D9/Device/ExclusiveSupersample.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Hooks/GameHook.h"

namespace {

constexpr int kGetBackBufferIndex = 18;

using GetBackBuffer_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, D3DBACKBUFFER_TYPE,
	IDirect3DSurface9**);

GameHook<GetBackBuffer_t> g_getBackBufferHook("IDirect3DDevice9::GetBackBuffer");

IDirect3DDevice9* g_device = nullptr;
IDirect3DSurface9* g_target = nullptr;
IDirect3DSurface9* g_depth = nullptr;
ExclusivePlan::Size g_drawn = {};
ExclusivePlan::Size g_display = {};
bool g_presenting = false;
bool g_copyFailureLogged = false;

template <typename T>
void ReleaseAndClear(T*& object)
{
	if (object == nullptr)
		return;

	object->Release();
	object = nullptr;
}

ExclusivePlan::Size Wanted()
{
	if (g_modVals.presentWidth <= 0 || g_modVals.presentHeight <= 0)
		return {};

	return { static_cast<UINT>(g_modVals.presentWidth), static_cast<UINT>(g_modVals.presentHeight) };
}

HRESULT RealBackBuffer(IDirect3DDevice9* device, IDirect3DSurface9** out)
{
	if (g_getBackBufferHook.IsLive())
		return g_getBackBufferHook.Original()(device, 0, 0, D3DBACKBUFFER_TYPE_MONO, out);

	return device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, out);
}

HRESULT STDMETHODCALLTYPE HookedGetBackBuffer(IDirect3DDevice9* device, UINT swapChain, UINT index,
	D3DBACKBUFFER_TYPE type, IDirect3DSurface9** out)
{
	if (g_target == nullptr || g_presenting || device != g_device || swapChain != 0 || index != 0 || out == nullptr)
		return g_getBackBufferHook.Original()(device, swapChain, index, type, out);

	g_target->AddRef();
	*out = g_target;
	return D3D_OK;
}

bool HookGetBackBuffer(IDirect3DDevice9* device)
{
	if (g_getBackBufferHook.IsLive())
		return true;

	void** const vtable = *reinterpret_cast<void***>(device);

	return g_getBackBufferHook.Install(vtable[kGetBackBufferIndex], &HookedGetBackBuffer);
}

bool CreateTargets(IDirect3DDevice9* device, ExclusivePlan::Size drawn, const D3DPRESENT_PARAMETERS& parameters)
{
	if (FAILED(device->CreateRenderTarget(drawn.width, drawn.height, parameters.BackBufferFormat,
		D3DMULTISAMPLE_NONE, 0, FALSE, &g_target, nullptr)))
	{
		g_target = nullptr;
		return false;
	}

	if (!parameters.EnableAutoDepthStencil)
		return true;

	const BOOL discard = (parameters.Flags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL) != 0;

	if (SUCCEEDED(device->CreateDepthStencilSurface(drawn.width, drawn.height, parameters.AutoDepthStencilFormat,
		D3DMULTISAMPLE_NONE, 0, discard, &g_depth, nullptr)))
	{
		return true;
	}

	g_depth = nullptr;
	ReleaseAndClear(g_target);
	return false;
}

void Bind(IDirect3DDevice9* device)
{
	device->SetRenderTarget(0, g_target);

	if (g_depth != nullptr)
		device->SetDepthStencilSurface(g_depth);
}

void Forget()
{
	g_drawn = {};
	g_display = {};
}

}

ExclusiveSupersample::Translation ExclusiveSupersample::Prepare(D3DPRESENT_PARAMETERS& parameters)
{
	const ExclusivePlan::Size display = ExclusivePlan::DisplayFor(parameters, g_drawn, g_display);
	parameters.BackBufferWidth = display.width;
	parameters.BackBufferHeight = display.height;

	const ExclusivePlan::Size wanted = Wanted();

	if (!ExclusivePlan::Wants(parameters, wanted))
		return {};

	parameters.MultiSampleType = D3DMULTISAMPLE_NONE;
	parameters.MultiSampleQuality = 0;
	return { true, wanted };
}

void ExclusiveSupersample::Finish(IDirect3DDevice9* device, const Translation& translation,
	D3DPRESENT_PARAMETERS& parameters)
{
	if (!translation.proxied || device == nullptr)
	{
		Forget();
		return;
	}

	if (!HookGetBackBuffer(device) || !CreateTargets(device, translation.drawn, parameters))
	{
		LOG("ExclusiveSupersample: no %ux%u surface could be made, the game draws at %ux%u",
			translation.drawn.width, translation.drawn.height, parameters.BackBufferWidth,
			parameters.BackBufferHeight);
		Forget();
		return;
	}

	g_device = device;
	g_display = { parameters.BackBufferWidth, parameters.BackBufferHeight };
	g_drawn = translation.drawn;
	Bind(device);

	parameters.BackBufferWidth = g_drawn.width;
	parameters.BackBufferHeight = g_drawn.height;

	LOG("ExclusiveSupersample: the game draws at %ux%u, shown at %ux%u in exclusive fullscreen", g_drawn.width,
		g_drawn.height, g_display.width, g_display.height);
}

void ExclusiveSupersample::OnDeviceLost(IDirect3DDevice9* device)
{
	if (g_target == nullptr || device != g_device)
		return;

	IDirect3DSurface9* backBuffer = nullptr;

	if (SUCCEEDED(RealBackBuffer(device, &backBuffer)) && backBuffer != nullptr)
	{
		device->SetRenderTarget(0, backBuffer);
		backBuffer->Release();
	}

	g_presenting = false;
	device->SetDepthStencilSurface(nullptr);
	ReleaseAndClear(g_depth);
	ReleaseAndClear(g_target);
}

void ExclusiveSupersample::BeginPresent(IDirect3DDevice9* device)
{
	if (g_target == nullptr || device != g_device)
		return;

	IDirect3DSurface9* backBuffer = nullptr;

	if (FAILED(RealBackBuffer(device, &backBuffer)) || backBuffer == nullptr)
		return;

	const HRESULT copied = device->StretchRect(g_target, nullptr, backBuffer, nullptr, D3DTEXF_LINEAR);

	if (FAILED(copied) && !g_copyFailureLogged)
	{
		g_copyFailureLogged = true;
		LOG("ExclusiveSupersample: copying the %ux%u drawing to the screen failed (0x%08lx), the screen stays black",
			g_drawn.width, g_drawn.height, static_cast<unsigned long>(copied));
	}

	device->SetRenderTarget(0, backBuffer);
	backBuffer->Release();
	g_presenting = true;
}

void ExclusiveSupersample::EndPresent(IDirect3DDevice9* device)
{
	if (!g_presenting || device != g_device)
		return;

	g_presenting = false;

	if (g_target != nullptr)
		device->SetRenderTarget(0, g_target);
}

bool ExclusiveSupersample::IsActive()
{
	return g_target != nullptr;
}
