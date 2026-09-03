#include "D3D9/DeviceHooks.h"

#include "Core/Profiler.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/CharaSounds.h"
#include "Game/GameOffsets.h"
#include "D3D9/PresentTuning.h"
#include "D3D9/Post/SceneUpscale.h"
#include "D3D9/SceneScale.h"
#include "D3D9/Post/PostChain.h"
#include "Game/GameState.h"
#include "Game/KeyboardSeat.h"
#include "Game/ReplayFiles.h"
#include "Game/CharaSelectProbe.h"
#include "Game/SceneWatch.h"
#include "Game/PotatoMode.h"
#include "Game/MemoryMap.h"
#include "Game/OnlineState.h"
#include "Game/BalanceRules.h"
#include "Game/GameRestart.h"
#include "Game/DataSearchPath.h"
#include "Game/GamePatches.h"
#include "Game/PatchPacks.h"
#include "Game/MusicRefresh.h"
#include "Game/OstImport.h"
#include "Game/VoiceImport.h"
#include "Game/SoundpackTransfer.h"
#include "Game/UserMusic.h"
#include "Network/NetplayTick.h"
#include "Game/ReplayState.h"
#include "Hooks/HookManager.h"
#include "Hooks/InputProbe.h"
#include "Game/ExtraStages.h"
#include "Game/SoundPacks.h"
#include "Game/ModFiles.h"
#include "Training/StageColor.h"
#include "D3D9/FrozenFrame.h"
#include "Network/PaletteShare.h"
#include "Web/UpdateInstall.h"
#include "Palette/PaletteBinder.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/PaletteSeat.h"
#include "Palette/EffectOwner.h"
#include "Palette/PaletteChoice.h"
#include "Palette/PaletteControl.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteIdentity.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteTexture.h"
#include "Screens/ScreenDirector.h"
#include "Overlay/FrameMeterHud.h"
#include "Training/PlayerControl.h"
#include "Overlay/WindowManager.h"
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

Reset_t oReset = nullptr;
Present_t oPresent = nullptr;
SetTexture_t oSetTexture = nullptr;
Clear_t oClear = nullptr;
DrawPrimitive_t oDrawPrimitive = nullptr;
DrawIndexedPrimitive_t oDrawIndexedPrimitive = nullptr;
DrawPrimitiveUP_t oDrawPrimitiveUP = nullptr;
DrawIndexedPrimitiveUP_t oDrawIndexedPrimitiveUP = nullptr;

volatile LONG g_presentCount = 0;

IDirect3DDevice9* g_device = nullptr;
D3DPRESENT_PARAMETERS g_presentParameters = {};
bool g_installed = false;

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

		WindowManager::GetInstance().OnDeviceLost();
		FrozenFrame::OnDeviceLost();
		ScreenDirector::OnDeviceLost();
		FrameMeterHud::OnDeviceLost();
		PaletteTexture::OnDeviceLost();
		PostChain::OnDeviceLost();
		SceneUpscale::OnDeviceLost();
	}

	HRESULT result = oReset(device, presentParameters);

	if (FAILED(result) && tunable && memcmp(&asked, presentParameters, sizeof(asked)) != 0)
	{
		LOG("Reset refused the tuned parameters (0x%08lx), retrying with the game's own",
			static_cast<unsigned long>(result));

		*presentParameters = asked;
		result = oReset(device, presentParameters);
	}

	if (isTrackedDevice)
	{
		if (SUCCEEDED(result))
		{
			if (presentParameters != nullptr)
				g_presentParameters = *presentParameters;

			WindowManager::GetInstance().OnDeviceReset();
			FrozenFrame::OnDeviceReset(device, g_presentParameters.BackBufferWidth,
				g_presentParameters.BackBufferHeight, g_presentParameters.BackBufferFormat);
		}
		else
		{
			LOG("Reset failed: 0x%08lx", result);
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

	if (device == g_device && (flags & D3DCLEAR_TARGET) != 0 && count == 0 &&
		(recolouring || stillLearning))
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

	return oClear(device, count, rects, flags, color, z, stencil);
}

HRESULT STDMETHODCALLTYPE HookedSetTexture(IDirect3DDevice9* device, DWORD stage,
	IDirect3DBaseTexture9* texture)
{
	const bool paletteShaped = PaletteTexture::OnSetTexture(device, stage, texture);
	PaletteDrawProbe::OnSetTexture(stage, texture, paletteShaped);
	return oSetTexture(device, stage, SceneUpscale::OnSetTexture(device, stage, texture));
}

HRESULT STDMETHODCALLTYPE HookedDrawPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
	UINT startVertex, UINT primitiveCount)
{
	PaletteDrawProbe::OnDraw();
	return oDrawPrimitive(device, type, startVertex, primitiveCount);
}

HRESULT STDMETHODCALLTYPE HookedDrawIndexedPrimitive(IDirect3DDevice9* device,
	D3DPRIMITIVETYPE type, INT baseVertexIndex, UINT minVertexIndex, UINT numVertices,
	UINT startIndex, UINT primitiveCount)
{
	PaletteDrawProbe::OnDraw();
	return oDrawIndexedPrimitive(device, type, baseVertexIndex, minVertexIndex, numVertices,
		startIndex, primitiveCount);
}

HRESULT STDMETHODCALLTYPE HookedDrawPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
	UINT primitiveCount, const void* vertexData, UINT stride)
{
	PaletteDrawProbe::OnDraw();
	return oDrawPrimitiveUP(device, type, primitiveCount, vertexData, stride);
}

HRESULT STDMETHODCALLTYPE HookedDrawIndexedPrimitiveUP(IDirect3DDevice9* device,
	D3DPRIMITIVETYPE type, UINT minVertexIndex, UINT numVertices, UINT primitiveCount,
	const void* indexData, D3DFORMAT indexFormat, const void* vertexData, UINT stride)
{
	PaletteDrawProbe::OnDraw();
	return oDrawIndexedPrimitiveUP(device, type, minVertexIndex, numVertices, primitiveCount,
		indexData, indexFormat, vertexData, stride);
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

	SceneWatch::OnFrame();

	if (!ScreenDirector::kOnHold)
		CharaSelectProbe::OnFrame();

	MemoryMap::InvalidateEffectSlotCache();

	if (device == g_device)
	{
		if (g_clearLearnFrames < kClearLearnFrames)
			++g_clearLearnFrames;

		{
			Profiler::Scope scope(Profiler::Section_PresentOnline);
			OnlineState::Update();
			NetplayTick::Update();
			GamePatches::Update();
			DataSearchPath::Assert();
			PatchPacks::OnFrame();
			UpdateInstall::OnFrame();
			BalanceRules::OnFrame();
			GameRestart::OnFrame();
			OstImport::Update();
			VoiceImport::Update();
			SoundpackTransfer::Update();

			if (UserMusic::ConsumeChanged())
				MusicRefresh::Reindex();

			CharaSounds::Update();

			if (SoundPacks::ConsumeScanRequest())
				SoundPacks::Scan();

			if (SoundPacks::ConsumeChanged())
				ModFiles::Rescan();
		}

		{
			Profiler::Scope scope(Profiler::Section_PresentReplay);
			ReplayState::Update();
		}

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
			FrameMeterHud::Render(device);
		}

		{
			Profiler::Scope scope(Profiler::Section_PresentOverlay);
			WindowManager::GetInstance().Render();
		}
	}

	PlayerControl::Update();
	KeyboardSeat::Update();
	ReplayFiles::Update();

	{
		Profiler::Scope scope(Profiler::Section_PresentPalette);

		PaletteControl::OnFrame();

		PaletteSeat::OnFrame();
		PalettePaint::OnFrame();

		EffectOwner::OnFrame();

		PaletteChoice::OnFrame();

		PaletteTexture::OnFrame();
		if (g_modVals.showLegacyPalettes)
		{
			PaletteDrawProbe::OnFrame();
			PaletteBinder::OnFrame();
			PaletteIdentity::OnFrame();
			PaletteManager::OnFrame();
		}
	}

	{
		Profiler::Scope scope(Profiler::Section_PresentShare);
		PaletteShare::OnFrame();
	}

	SceneScale::OnFrame();
	SceneUpscale::OnPresent();

	InputProbe::OnFrame();
	StageColor::OnFrame();
	ExtraStages::OnFrame();
	PotatoMode::OnFrame();

	HRESULT result = D3D_OK;

	{
		Profiler::Scope scope(Profiler::Section_PresentDevice);
		result = oPresent(device, sourceRect, destRect, destWindowOverride, dirtyRegion);
	}

	if (device == g_device)
		Profiler::EndPresentFrame();

	return result;
}

bool HookVTableEntry(void** vtable, int index, void* detour, void** original, const char* label)
{
	return HookManager::CreateAndEnableHook(vtable[index], detour, original, label);
}

}

bool DeviceHooks::Install(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS& presentParameters, HWND focusWindow)
{
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

	void** vtable = *reinterpret_cast<void***>(device);

	const bool reset = HookVTableEntry(vtable, kResetIndex, &HookedReset,
		reinterpret_cast<void**>(&oReset), "IDirect3DDevice9::Reset");
	const bool present = HookVTableEntry(vtable, kPresentIndex, &HookedPresent,
		reinterpret_cast<void**>(&oPresent), "IDirect3DDevice9::Present");

	HookVTableEntry(vtable, kSetTextureIndex, &HookedSetTexture,
		reinterpret_cast<void**>(&oSetTexture), "IDirect3DDevice9::SetTexture");

	HookVTableEntry(vtable, kClearIndex, &HookedClear,
		reinterpret_cast<void**>(&oClear), "IDirect3DDevice9::Clear");

	HookVTableEntry(vtable, kDrawPrimitiveIndex, &HookedDrawPrimitive,
		reinterpret_cast<void**>(&oDrawPrimitive), "IDirect3DDevice9::DrawPrimitive");
	HookVTableEntry(vtable, kDrawIndexedPrimitiveIndex, &HookedDrawIndexedPrimitive,
		reinterpret_cast<void**>(&oDrawIndexedPrimitive), "IDirect3DDevice9::DrawIndexedPrimitive");
	HookVTableEntry(vtable, kDrawPrimitiveUPIndex, &HookedDrawPrimitiveUP,
		reinterpret_cast<void**>(&oDrawPrimitiveUP), "IDirect3DDevice9::DrawPrimitiveUP");
	HookVTableEntry(vtable, kDrawIndexedPrimitiveUPIndex, &HookedDrawIndexedPrimitiveUP,
		reinterpret_cast<void**>(&oDrawIndexedPrimitiveUP),
		"IDirect3DDevice9::DrawIndexedPrimitiveUP");

	if (!reset || !present)
	{
		LOG("Device hook installation incomplete (reset=%d present=%d)", reset, present);
		return false;
	}

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
