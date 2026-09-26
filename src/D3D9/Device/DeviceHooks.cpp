#include "D3D9/Device/DeviceHooks.h"
#include "Core/ThreadRole.h"
#include "Core/Boot/Modules.h"

#include "Core/Profiler.h"
#include "Core/Boot/crashdump.h"
#include "Core/Config/interfaces.h"
#include "Core/Harness/CleanFrame.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Device/GraphicsWrapper.h"
#include "Game/Engine/GameOffsets.h"
#include "D3D9/Device/PresentTuning.h"
#include "D3D9/Post/SceneUpscale.h"
#include "D3D9/Draw/DrawTrace.h"
#include "D3D9/Post/PostChain.h"
#include "Game/Engine/GameState.h"
#include "Game/Stages/BgGrade.h"
#include "Game/Stages/BgClear.h"
#include "Game/Stages/BgVertexProbe.h"
#include "Network/NetLink.h"
#include "Hooks/GameHook.h"
#include "Training/StageColor.h"
#include "D3D9/Draw/FrozenFrame.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/PaletteTexture.h"
#include "Screens/ScreenDirector.h"
#include "Overlay/Framework/WindowManager.h"
#include "Training/FrameStepper.h"

#include <MinHook.h>
#include <cstring>

namespace {

constexpr int kResetIndex = 16;
constexpr int kPresentIndex = 17;
constexpr unsigned kSceneWidth = 1280;
constexpr unsigned kSceneHeight = 720;

constexpr int kClearIndex = 43;
constexpr int kSetTextureIndex = 65;
constexpr int kDrawPrimitiveIndex = 81;
constexpr int kDrawIndexedPrimitiveIndex = 82;
constexpr int kDrawPrimitiveUPIndex = 83;
constexpr int kDrawIndexedPrimitiveUPIndex = 84;

using Reset_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using Present_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using SetTexture_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
using Clear_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD,
	D3DCOLOR, float, DWORD);
using DrawPrimitive_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
using DrawIndexedPrimitive_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE,
	INT, UINT, UINT, UINT, UINT);
using DrawPrimitiveUP_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT,
	const void*, UINT);
using DrawIndexedPrimitiveUP_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE,
	UINT, UINT, UINT, const void*, D3DFORMAT, const void*, UINT);

GameHook<Reset_t> g_resetHook("IDirect3DDevice9::Reset");
GameHook<Present_t> g_presentHook("IDirect3DDevice9::Present");
GameHook<SetTexture_t> g_setTextureHook("IDirect3DDevice9::SetTexture");
GameHook<Clear_t> g_clearHook("IDirect3DDevice9::Clear");
GameHook<DrawPrimitive_t> g_drawPrimitiveHook("IDirect3DDevice9::DrawPrimitive");
GameHook<DrawIndexedPrimitive_t> g_drawIndexedPrimitiveHook("IDirect3DDevice9::DrawIndexedPrimitive");
GameHook<DrawPrimitiveUP_t> g_drawPrimitiveUPHook("IDirect3DDevice9::DrawPrimitiveUP");
GameHook<DrawIndexedPrimitiveUP_t> g_drawIndexedPrimitiveUPHook("IDirect3DDevice9::DrawIndexedPrimitiveUP");

volatile LONG g_presentCount = 0;
volatile LONG g_deviceLost = 0;
volatile LONG g_resetGeneration = 0;

IDirect3DDevice9* g_device = nullptr;
D3DPRESENT_PARAMETERS g_presentParameters = {};
bool g_installed = false;

const char* HeldDefaultResources()
{
	static char text[160];

	const int palettes = PaletteTexture::HeldVolatileCount();

	snprintf(text, sizeof(text), "%s%s%s%s%s%s",
		WindowManager::GetInstance().HoldsDeviceResources() ? "overlay " : "",
		FrozenFrame::HoldsDeviceResources() ? "frozen-frame " : "",
		QuadRenderer::HoldsDeviceResources() ? "quads " : "",
		PostChain::HoldsDeviceResources() ? "post-chain " : "",
		SceneUpscale::HoldsDeviceResources() ? "upscale " : "",
		palettes > 0 ? "palette-textures" : "");

	return text[0] != 0 ? text : "none of the mod's";
}

HRESULT STDMETHODCALLTYPE HookedReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentParameters)
{
	const bool isTrackedDevice = (device == g_device);

	D3DPRESENT_PARAMETERS asked = {};
	const bool tunable = isTrackedDevice && presentParameters != nullptr;

	if (tunable)
	{
		asked = *presentParameters;
		PresentTuning::Apply(device, *presentParameters);
	}

	if (isTrackedDevice)
	{
		LOG("Reset requested: %ux%u windowed=%d",
			presentParameters ? presentParameters->BackBufferWidth : 0,
			presentParameters ? presentParameters->BackBufferHeight : 0,
			presentParameters ? presentParameters->Windowed : -1);

		InterlockedExchange(&g_deviceLost, 1);

		WindowManager::GetInstance().OnDeviceLost();
		FrozenFrame::OnDeviceLost();
		QuadRenderer::OnDeviceLost();
		PaletteTexture::OnDeviceLost();
		PostChain::OnDeviceLost();
		SceneUpscale::OnDeviceLost();
	}

	HRESULT result = g_resetHook.Original()(device, presentParameters);

	if (FAILED(result) && tunable && memcmp(&asked, presentParameters, sizeof(asked)) != 0)
	{
		LOG("Reset refused the tuned parameters (0x%08lx), retrying with the game's own",
			static_cast<unsigned long>(result));

		*presentParameters = asked;
		result = g_resetHook.Original()(device, presentParameters);
	}

	if (isTrackedDevice)
	{
		if (SUCCEEDED(result))
		{
			if (presentParameters != nullptr)
				g_presentParameters = *presentParameters;

			InterlockedIncrement(&g_resetGeneration);
			InterlockedExchange(&g_deviceLost, 0);

			WindowManager::GetInstance().OnDeviceReset();
			FrozenFrame::OnDeviceReset(device, g_presentParameters.BackBufferWidth,
				g_presentParameters.BackBufferHeight, g_presentParameters.BackBufferFormat);
		}
		else
		{
			LOG("Reset failed: 0x%08lx, still held: %s", static_cast<unsigned long>(result),
				HeldDefaultResources());
		}
	}

	return result;
}

bool TargetIsFullFrame(IDirect3DDevice9* device, unsigned& outWidth, unsigned& outHeight,
	bool& outIsBackBuffer)
{
	outWidth = 0;
	outHeight = 0;
	outIsBackBuffer = false;

	IDirect3DSurface9* target = nullptr;
	if (FAILED(device->GetRenderTarget(0, &target)) || target == nullptr)
		return false;

	D3DSURFACE_DESC desc = {};
	const bool described = SUCCEEDED(target->GetDesc(&desc));

	IDirect3DSurface9* backBuffer = nullptr;
	if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) &&
		backBuffer != nullptr)
	{
		outIsBackBuffer = target == backBuffer;
		backBuffer->Release();
	}

	target->Release();

	if (!described)
		return false;

	outWidth = desc.Width;
	outHeight = desc.Height;

	const bool presentSized = desc.Width == g_presentParameters.BackBufferWidth &&
		desc.Height == g_presentParameters.BackBufferHeight;

	const bool referenceSized = desc.Width == kSceneWidth && desc.Height == kSceneHeight;

	return presentSized || referenceSized;
}

struct ClearSignature
{
	DWORD flags;
	D3DCOLOR color;
	unsigned width;
	unsigned height;
	bool backBuffer;
};

constexpr int kMaxClearSignatures = 16;
constexpr int kClearLearnFrames = 600;

ClearSignature g_clearSeen[kMaxClearSignatures] = {};
int g_clearSeenCount = 0;
int g_clearLearnFrames = 0;

void NoteClear(const ClearSignature& seen)
{
	for (int i = 0; i < g_clearSeenCount; ++i)
	{
		const ClearSignature& known = g_clearSeen[i];
		if (known.flags == seen.flags && known.color == seen.color && known.width == seen.width &&
			known.height == seen.height && known.backBuffer == seen.backBuffer)
		{
			return;
		}
	}

	if (g_clearSeenCount >= kMaxClearSignatures)
		return;

	g_clearSeen[g_clearSeenCount++] = seen;
	LOG("clear: flags 0x%lx colour 0x%08lx target %ux%u%s", seen.flags,
		static_cast<unsigned long>(seen.color), seen.width, seen.height,
		seen.backBuffer ? " (back buffer)" : "");
}

HRESULT STDMETHODCALLTYPE HookedClear(IDirect3DDevice9* device, DWORD count, const D3DRECT* rects,
	DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
	const bool recolouring = StageColor::IsEnabled();
	const bool stillLearning = g_clearSeenCount < kMaxClearSignatures &&
		g_clearLearnFrames < kClearLearnFrames;

	if (device == g_device && (flags & D3DCLEAR_TARGET) != 0)
	{
		if (count == 0 && (recolouring || stillLearning))
		{
			unsigned width = 0;
			unsigned height = 0;
			bool isBackBuffer = false;
			const bool fullFrame = TargetIsFullFrame(device, width, height, isBackBuffer);

			if (stillLearning)
				NoteClear({ flags, color, width, height, isBackBuffer });

			if (recolouring && fullFrame)
				color = StageColor::GetClearColor();
		}

		if (!recolouring)
			color = BgClear::Behind(color);
	}

	return g_clearHook.Original()(device, count, rects, flags, color, z, stencil);
}

HRESULT STDMETHODCALLTYPE HookedSetTexture(IDirect3DDevice9* device, DWORD stage,
	IDirect3DBaseTexture9* texture)
{
	const bool paletteShaped = PaletteTexture::OnSetTexture(device, stage, texture);
	PaletteDrawProbe::OnSetTexture(stage, texture, paletteShaped);
	CleanFrame::OnSetTexture(stage, paletteShaped);
	return g_setTextureHook.Original()(device, stage, SceneUpscale::OnSetTexture(device, stage, texture));
}

HRESULT STDMETHODCALLTYPE HookedDrawPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
	UINT startVertex, UINT primitiveCount)
{
	PaletteDrawProbe::OnDraw();
	DrawTrace::OnDraw(device, "dp", type, primitiveCount);
	if (CleanFrame::SkipsDraw(device))
		return D3D_OK;

	return g_drawPrimitiveHook.Original()(device, type, startVertex, primitiveCount);
}

HRESULT STDMETHODCALLTYPE HookedDrawIndexedPrimitive(IDirect3DDevice9* device,
	D3DPRIMITIVETYPE type, INT baseVertexIndex, UINT minVertexIndex, UINT numVertices,
	UINT startIndex, UINT primitiveCount)
{
	PaletteDrawProbe::OnDraw();
	BgVertexProbe::OnDraw(device);
	DrawTrace::OnIndexedDraw(device, type, primitiveCount, baseVertexIndex, minVertexIndex,
		numVertices);
	if (CleanFrame::SkipsDraw(device))
		return D3D_OK;

	return g_drawIndexedPrimitiveHook.Original()(device, type, baseVertexIndex, minVertexIndex, numVertices,
		startIndex, primitiveCount);
}

HRESULT STDMETHODCALLTYPE HookedDrawPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
	UINT primitiveCount, const void* vertexData, UINT stride)
{
	PaletteDrawProbe::OnDraw();
	DrawTrace::OnDraw(device, "dpup", type, primitiveCount, vertexData, stride);
	if (CleanFrame::SkipsDraw(device))
		return D3D_OK;

	return g_drawPrimitiveUPHook.Original()(device, type, primitiveCount, vertexData, stride);
}

HRESULT STDMETHODCALLTYPE HookedDrawIndexedPrimitiveUP(IDirect3DDevice9* device,
	D3DPRIMITIVETYPE type, UINT minVertexIndex, UINT numVertices, UINT primitiveCount,
	const void* indexData, D3DFORMAT indexFormat, const void* vertexData, UINT stride)
{
	PaletteDrawProbe::OnDraw();
	DrawTrace::OnDraw(device, "dipup", type, primitiveCount, vertexData, stride, numVertices);
	if (CleanFrame::SkipsDraw(device))
		return D3D_OK;

	return g_drawIndexedPrimitiveUPHook.Original()(device, type, minVertexIndex, numVertices, primitiveCount,
		indexData, indexFormat, vertexData, stride);
}

constexpr int kOverlayRetryFrames = 120;
constexpr int kOverlayRetryAttempts = 10;

int g_overlayRetryIn = kOverlayRetryFrames;
int g_overlayAttempts = 0;

HWND ResolveGameWindow(IDirect3DDevice9* device)
{
	if (g_gameProc.hWndGame != nullptr)
		return g_gameProc.hWndGame;

	uint32_t handle = 0;

	if (TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kWindowHandle)), handle) &&
		handle != 0)
	{
		return reinterpret_cast<HWND>(static_cast<uintptr_t>(handle));
	}

	D3DDEVICE_CREATION_PARAMETERS creation = {};

	if (SUCCEEDED(device->GetCreationParameters(&creation)) && creation.hFocusWindow != nullptr)
		return creation.hFocusWindow;

	return g_presentParameters.hDeviceWindow;
}

void RetryOverlay(IDirect3DDevice9* device)
{
	WindowManager& manager = WindowManager::GetInstance();

	if (manager.IsInitialized() || g_overlayAttempts >= kOverlayRetryAttempts)
		return;

	if (--g_overlayRetryIn > 0)
		return;

	g_overlayRetryIn = kOverlayRetryFrames;
	++g_overlayAttempts;

	const HWND window = ResolveGameWindow(device);

	LOG("The overlay is not up, attempt %d of %d with window 0x%p", g_overlayAttempts,
		kOverlayRetryAttempts, static_cast<void*>(window));

	if (window == nullptr)
		return;

	g_gameProc.hWndGame = window;
	manager.Initialize(window, device);
}

void ReportModWork(const LARGE_INTEGER& start)
{
	static LARGE_INTEGER frequency = {};

	if (frequency.QuadPart == 0)
		QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER now = {};
	QueryPerformanceCounter(&now);

	NetLink::OnPresent((now.QuadPart - start.QuadPart) * 1000000 / frequency.QuadPart);
}

bool FrozenFrameCouldBeReplayed()
{
	return FrameStepper::IsImplemented() && GameState::AllowsTrainingTools() &&
		FrameStepper::SuppressesTicks();
}

HRESULT STDMETHODCALLTYPE HookedPresent(IDirect3DDevice9* device, const RECT* sourceRect,
	const RECT* destRect, HWND destWindowOverride, const RGNDATA* dirtyRegion)
{
	InterlockedIncrement(&g_presentCount);
	ThreadRole::Mark(ThreadRole::Role_Render);

	LARGE_INTEGER modStart = {};
	QueryPerformanceCounter(&modStart);

	if (device == g_device && InterlockedCompareExchange(&g_deviceLost, 0, 0) != 0)
	{
		ReportModWork(modStart);

		const HRESULT lost = g_presentHook.Original()(device, sourceRect, destRect, destWindowOverride, dirtyRegion);

		if (lost != D3DERR_DEVICELOST)
			InterlockedExchange(&g_deviceLost, 0);

		Profiler::EndPresentFrame();
		return lost;
	}

	DrawTrace::OnPresentBegin(sourceRect, destRect);
	GraphicsWrapper::Detect(device);
	Modules::Run(Modules::Group_PresentBegin, device);

	if (device == g_device)
	{
		if (g_clearLearnFrames < kClearLearnFrames)
			++g_clearLearnFrames;

		KeepCrashHandler();
		Modules::Run(Modules::Group_Frame, device);
		Modules::Run(Modules::Group_Replay, device);

		const bool stepped = FrameStepper::ConsumeSteppedFlag();

		{
			Profiler::Scope scope(Profiler::Section_PresentFrozenFrame);

			if (FrameStepper::NeedsFrozenFrameReplay() && !stepped)
				FrozenFrame::Draw(device);
			else if (FrozenFrameCouldBeReplayed())
				FrozenFrame::Capture(device);
		}

		PostChain::Apply(device);

		{
			Profiler::Scope scope(Profiler::Section_PresentMeterHud);

			ScreenDirector::Render(device);

			if (QuadRenderer::Begin(device))
			{
				Modules::Run(Modules::Group_Hud, device);
				QuadRenderer::End();
			}
		}

		{
			Profiler::Scope scope(Profiler::Section_PresentOverlay);
			RetryOverlay(device);
			WindowManager::GetInstance().Render();
		}
	}

	Modules::Run(Modules::Group_Input, device);
	Modules::Run(Modules::Group_Palette, device);
	Modules::Run(Modules::Group_Share, device);
	Modules::Run(Modules::Group_Tail, device);

	HRESULT result = D3D_OK;

	if (device == g_device)
		ReportModWork(modStart);

	DrawTrace::OnPresentEnd();
	CleanFrame::OnPresent();

	{
		Profiler::Scope scope(Profiler::Section_PresentDevice);
		result = g_presentHook.Original()(device, sourceRect, destRect, destWindowOverride, dirtyRegion);
	}

	if (device == g_device)
	{
		if (result == D3DERR_DEVICELOST)
			InterlockedExchange(&g_deviceLost, 1);

		Profiler::EndPresentFrame();
	}

	return result;
}

template <typename Fn, typename Handler>
bool HookVTableEntry(void** vtable, int index, GameHook<Fn>& hook, Handler handler)
{
	if (vtable == nullptr || !IsReadableMemory(vtable + index, sizeof(void*)))
	{
		LOG("Device hooks: '%s' skipped, slot %d of vtable 0x%p is not readable", hook.Label(), index,
			static_cast<void*>(vtable));

		return false;
	}

	void* const target = vtable[index];

	if (target == nullptr || !IsReadableMemory(target, 1))
	{
		LOG("Device hooks: '%s' skipped, slot %d holds 0x%p", hook.Label(), index, target);
		return false;
	}

	return hook.Install(target, handler);
}

}

bool DeviceHooks::Install(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS& presentParameters, HWND focusWindow)
{
	LOG("Device hooks: asked to install on device 0x%p, window 0x%p", static_cast<void*>(device),
		static_cast<void*>(focusWindow));

	if (g_installed)
	{
		g_device = device;
		g_presentParameters = presentParameters;
		return true;
	}

	if (device == nullptr)
		return false;

	g_device = device;
	g_presentParameters = presentParameters;

	if (focusWindow != nullptr)
		g_gameProc.hWndGame = focusWindow;

	if (!IsReadableMemory(device, sizeof(void*)))
	{
		LOG("Device hooks: device 0x%p is not readable, no device hooks installed",
			static_cast<void*>(device));

		return false;
	}

	void** vtable = *reinterpret_cast<void***>(device);

	if (vtable == nullptr
		|| !IsReadableMemory(vtable, (kDrawIndexedPrimitiveUPIndex + 1) * sizeof(void*)))
	{
		LOG("Device hooks: vtable 0x%p of device 0x%p is too short or unreadable, no device hooks "
			"installed", static_cast<void*>(vtable), static_cast<void*>(device));

		return false;
	}

	const bool reset = HookVTableEntry(vtable, kResetIndex, g_resetHook, &HookedReset);
	const bool present = HookVTableEntry(vtable, kPresentIndex, g_presentHook, &HookedPresent);

	HookVTableEntry(vtable, kSetTextureIndex, g_setTextureHook, &HookedSetTexture);

	HookVTableEntry(vtable, kClearIndex, g_clearHook, &HookedClear);

	HookVTableEntry(vtable, kDrawPrimitiveIndex, g_drawPrimitiveHook, &HookedDrawPrimitive);
	HookVTableEntry(vtable, kDrawIndexedPrimitiveIndex, g_drawIndexedPrimitiveHook, &HookedDrawIndexedPrimitive);
	HookVTableEntry(vtable, kDrawPrimitiveUPIndex, g_drawPrimitiveUPHook, &HookedDrawPrimitiveUP);
	HookVTableEntry(vtable, kDrawIndexedPrimitiveUPIndex, g_drawIndexedPrimitiveUPHook, &HookedDrawIndexedPrimitiveUP);

	if (!reset || !present)
	{
		LOG("Device hook installation incomplete (reset=%d present=%d)", reset, present);
		return false;
	}

	BgGrade::Initialize();
	BgGrade::Attach(device);

	g_installed = true;
	LOG("Device hooks installed on device 0x%p, window 0x%p", (void*)device, (void*)g_gameProc.hWndGame);

	WindowManager::GetInstance().Initialize(g_gameProc.hWndGame, device);
	FrozenFrame::OnDeviceReset(device, g_presentParameters.BackBufferWidth,
		g_presentParameters.BackBufferHeight, g_presentParameters.BackBufferFormat);
	return true;
}

bool DeviceHooks::IsInstalled()
{
	return g_installed;
}

unsigned long DeviceHooks::PresentCount()
{
	return static_cast<unsigned long>(InterlockedCompareExchange(&g_presentCount, 0, 0));
}

bool DeviceHooks::IsDeviceUsable()
{
	return InterlockedCompareExchange(&g_deviceLost, 0, 0) == 0;
}

unsigned long DeviceHooks::ResetGeneration()
{
	return static_cast<unsigned long>(InterlockedCompareExchange(&g_resetGeneration, 0, 0));
}

IDirect3DDevice9* DeviceHooks::GetDevice()
{
	return g_device;
}

float DeviceHooks::GetOverlayScale()
{
	static float cached = 1.0f;
	static DWORD cachedTick = 0;

	const DWORD now = GetTickCount();
	if (cachedTick != 0 && now - cachedTick < 200)
		return cached;

	cachedTick = now;
	cached = 1.0f;

	if (g_presentParameters.BackBufferWidth == 0 || g_gameProc.hWndGame == nullptr)
		return cached;

	RECT client = {};
	if (!GetClientRect(g_gameProc.hWndGame, &client) || client.right <= 0)
		return cached;

	cached = static_cast<float>(g_presentParameters.BackBufferWidth) /
		static_cast<float>(client.right);

	if (!(cached > 0.1f) || cached > 8.0f)
		cached = 1.0f;

	return cached;
}

const D3DPRESENT_PARAMETERS& DeviceHooks::GetPresentParameters()
{
	return g_presentParameters;
}
