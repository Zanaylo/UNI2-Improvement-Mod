#include "D3D9/Device/ScaledTargets.h"

#include "Core/logger.h"
#include "D3D9/Device/ScaledDeviceState.h"
#include "D3D9/Device/TargetSizeMask.h"
#include "Game/Display/InternalResolution.h"
#include "Game/Display/PotatoMode.h"
#include "Game/Display/PotatoStage.h"
#include "Hooks/GameHook.h"

#include <Windows.h>

#include <cstdio>
#include <new>

namespace {

constexpr GUID kReleaseNoticeKey = { 0x4254a3d6, 0xf091, 0x4641, { 0xad, 0xa5, 0x0e, 0x41, 0xd2, 0x51, 0x47, 0xb6 } };

constexpr int kCreateTextureIndex = 23;
constexpr int kCreateRenderTargetIndex = 28;
constexpr int kCreateDepthStencilSurfaceIndex = 29;
constexpr int kSetRenderTargetIndex = 37;

constexpr int kMaxTargets = 32;
constexpr D3DFORMAT kSceneLayerFormat = D3DFMT_X8R8G8B8;

using CreateTexture_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD,
	D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
using CreateSurface_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT,
	D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
using SetRenderTarget_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);

GameHook<CreateTexture_t> g_createTextureHook("IDirect3DDevice9::CreateTexture");
GameHook<CreateSurface_t> g_createRenderTargetHook("IDirect3DDevice9::CreateRenderTarget");
GameHook<CreateSurface_t> g_createDepthStencilHook("IDirect3DDevice9::CreateDepthStencilSurface");
GameHook<SetRenderTarget_t> g_setRenderTargetHook("IDirect3DDevice9::SetRenderTarget");

struct Target
{
	const void* surface;
	const void* texture;
};

SRWLOCK g_lock = SRWLOCK_INIT;
Target g_targets[kMaxTargets] = {};
int g_targetCount = 0;

bool g_attached = false;
bool g_active = false;
unsigned g_width = InternalResolution::kBaseWidth;
unsigned g_height = InternalResolution::kBaseHeight;
float g_scaleX = 1.0f;
float g_scaleY = 1.0f;

bool g_boundScaled = false;
bool g_reportedNoReleaseNotice = false;

int g_scaledCreations = 0;
int g_refusedCreations = 0;

char g_status[192] = "off";

class SharedLock
{
public:
	SharedLock() { AcquireSRWLockShared(&g_lock); }
	~SharedLock() { ReleaseSRWLockShared(&g_lock); }

	SharedLock(const SharedLock&) = delete;
	SharedLock& operator=(const SharedLock&) = delete;
};

class ExclusiveLock
{
public:
	ExclusiveLock() { AcquireSRWLockExclusive(&g_lock); }
	~ExclusiveLock() { ReleaseSRWLockExclusive(&g_lock); }

	ExclusiveLock(const ExclusiveLock&) = delete;
	ExclusiveLock& operator=(const ExclusiveLock&) = delete;
};

bool IsStageFormat(D3DFORMAT format)
{
	return format != kSceneLayerFormat;
}

bool ShouldScale(UINT width, UINT height, D3DFORMAT format)
{
	return g_active && width == InternalResolution::kBaseWidth && height == InternalResolution::kBaseHeight &&
		IsStageFormat(format);
}

void ForgetLocked(const void* identity)
{
	for (int i = 0; i < g_targetCount; ++i)
	{
		if (g_targets[i].surface != identity && g_targets[i].texture != identity)
			continue;

		g_targets[i] = g_targets[--g_targetCount];
		--i;
	}
}

void Forget(const void* identity)
{
	if (identity == nullptr)
		return;

	ExclusiveLock lock;
	ForgetLocked(identity);
}

class ReleaseNotice final : public IUnknown
{
public:
	static bool Attach(IDirect3DResource9* resource)
	{
		ReleaseNotice* const notice = new (std::nothrow) ReleaseNotice(resource);

		if (notice == nullptr)
			return false;

		const HRESULT result = resource->SetPrivateData(kReleaseNoticeKey, static_cast<IUnknown*>(notice),
			sizeof(IUnknown*), D3DSPD_IUNKNOWN);

		if (FAILED(result))
			notice->m_identity = nullptr;

		notice->Release();
		return SUCCEEDED(result);
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override
	{
		if (object == nullptr)
			return E_POINTER;

		*object = nullptr;

		if (!IsEqualIID(id, IID_IUnknown))
			return E_NOINTERFACE;

		*object = static_cast<IUnknown*>(this);
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return static_cast<ULONG>(InterlockedIncrement(&m_references));
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		const LONG remaining = InterlockedDecrement(&m_references);

		if (remaining != 0)
			return static_cast<ULONG>(remaining);

		Forget(m_identity);
		delete this;
		return 0;
	}

private:
	explicit ReleaseNotice(const void* identity) : m_identity(identity) {}
	~ReleaseNotice() = default;

	const void* m_identity;
	LONG m_references = 1;
};

void ForgetOnRelease(IDirect3DResource9* resource)
{
	if (ReleaseNotice::Attach(resource) || g_reportedNoReleaseNotice)
		return;

	g_reportedNoReleaseNotice = true;
	LOG("[InternalResolution] the device keeps no private data, freed targets stay in the list (0x%p)",
		static_cast<void*>(resource));
}

void Remember(const void* surface, const void* texture)
{
	ExclusiveLock lock;
	ForgetLocked(surface);

	if (g_targetCount >= kMaxTargets)
	{
		LOG("[InternalResolution] more than %d scaled targets, the newest one is not tracked", kMaxTargets);
		return;
	}

	g_targets[g_targetCount++] = { surface, texture };
	++g_scaledCreations;
}

void ForgetLevelOf(IDirect3DTexture9* texture)
{
	IDirect3DSurface9* level = nullptr;

	if (FAILED(texture->GetSurfaceLevel(0, &level)) || level == nullptr)
		return;

	Forget(level);
	level->Release();
}

void RememberTexture(IDirect3DTexture9* texture)
{
	IDirect3DSurface9* level = nullptr;

	if (FAILED(texture->GetSurfaceLevel(0, &level)) || level == nullptr)
		return;

	TargetSizeMask::Watch(texture);
	TargetSizeMask::Watch(level);
	Remember(level, texture);
	level->Release();
	ForgetOnRelease(texture);
}

void ReportRefusal(const char* what, HRESULT result)
{
	++g_refusedCreations;
	LOG("[InternalResolution] %s at %ux%u refused (0x%08lx), kept at 1280x720", what, g_width, g_height,
		static_cast<unsigned long>(result));
}

HRESULT STDMETHODCALLTYPE HookedCreateTexture(IDirect3DDevice9* device, UINT width, UINT height,
	UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** texture, HANDLE* shared)
{
	const CreateTexture_t original = g_createTextureHook.Original();
	const bool renderTarget = (usage & D3DUSAGE_RENDERTARGET) != 0;

	if (!renderTarget || !ShouldScale(width, height, format))
	{
		const HRESULT result = original(device, width, height, levels, usage, format, pool, texture, shared);

		if (renderTarget && SUCCEEDED(result) && texture != nullptr && *texture != nullptr)
		{
			Forget(*texture);
			ForgetLevelOf(*texture);
		}

		return result;
	}

	const HRESULT scaled = original(device, g_width, g_height, levels, usage, format, pool, texture, shared);

	if (SUCCEEDED(scaled) && texture != nullptr && *texture != nullptr)
	{
		Forget(*texture);
		RememberTexture(*texture);
		LOG("[InternalResolution] render target texture format %d created at %ux%u (0x%p)",
			static_cast<int>(format), g_width, g_height, static_cast<void*>(*texture));
		return scaled;
	}

	ReportRefusal("render target texture", scaled);
	return original(device, width, height, levels, usage, format, pool, texture, shared);
}

HRESULT CreateScaledSurface(const CreateSurface_t original, const char* what, IDirect3DDevice9* device,
	UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multisample, DWORD quality, BOOL flag,
	IDirect3DSurface9** surface, HANDLE* shared)
{
	if (!ShouldScale(width, height, format))
	{
		const HRESULT result = original(device, width, height, format, multisample, quality, flag, surface,
			shared);

		if (SUCCEEDED(result) && surface != nullptr)
			Forget(*surface);

		return result;
	}

	const HRESULT scaled = original(device, g_width, g_height, format, multisample, quality, flag, surface,
		shared);

	if (SUCCEEDED(scaled) && surface != nullptr && *surface != nullptr)
	{
		TargetSizeMask::Watch(*surface);
		Remember(*surface, nullptr);
		ForgetOnRelease(*surface);
		LOG("[InternalResolution] %s format %d multisample %d created at %ux%u (0x%p)", what,
			static_cast<int>(format), static_cast<int>(multisample), g_width, g_height,
			static_cast<void*>(*surface));
		return scaled;
	}

	ReportRefusal(what, scaled);
	return original(device, width, height, format, multisample, quality, flag, surface, shared);
}

HRESULT STDMETHODCALLTYPE HookedCreateRenderTarget(IDirect3DDevice9* device, UINT width, UINT height,
	D3DFORMAT format, D3DMULTISAMPLE_TYPE multisample, DWORD quality, BOOL lockable,
	IDirect3DSurface9** surface, HANDLE* shared)
{
	return CreateScaledSurface(g_createRenderTargetHook.Original(), "render target", device, width, height,
		format, multisample, quality, lockable, surface, shared);
}

HRESULT STDMETHODCALLTYPE HookedCreateDepthStencilSurface(IDirect3DDevice9* device, UINT width,
	UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multisample, DWORD quality, BOOL discard,
	IDirect3DSurface9** surface, HANDLE* shared)
{
	return CreateScaledSurface(g_createDepthStencilHook.Original(), "depth stencil", device, width, height,
		format, multisample, quality, discard, surface, shared);
}

HRESULT STDMETHODCALLTYPE HookedSetRenderTarget(IDirect3DDevice9* device, DWORD index,
	IDirect3DSurface9* surface)
{
	const HRESULT result = g_setRenderTargetHook.Original()(device, index, surface);

	if (index == 0 && SUCCEEDED(result))
		g_boundScaled = ScaledTargets::IsScaled(surface);

	return result;
}

template <typename Fn, typename Handler>
void Hook(void** vtable, int index, GameHook<Fn>& hook, Handler handler)
{
	if (hook.IsLive() || hook.Install(vtable[index], handler))
		return;

	LOG("[InternalResolution] could not hook %s", hook.Label());
}

bool WantedSize(unsigned& width, unsigned& height)
{
	if (PotatoStage::GetSize(PotatoStage::GetLevel(), width, height))
		return true;

	return !PotatoMode::IsActive() && InternalResolution::GetSize(InternalResolution::GetLevel(), width, height);
}

void ChooseSize()
{
	g_active = WantedSize(g_width, g_height);
	g_scaleX = static_cast<float>(g_width) / static_cast<float>(InternalResolution::kBaseWidth);
	g_scaleY = static_cast<float>(g_height) / static_cast<float>(InternalResolution::kBaseHeight);
}

LONG Scale(LONG value, float scale)
{
	return static_cast<LONG>(static_cast<float>(value) * scale + (value < 0 ? -0.5f : 0.5f));
}

}

void ScaledTargets::Attach(IDirect3DDevice9* device)
{
	if (g_attached || device == nullptr)
		return;

	g_attached = true;
	ChooseSize();

	if (!g_active)
	{
		snprintf(g_status, sizeof(g_status), "off, the scene is drawn at 1280x720");
		return;
	}

	void** const vtable = *reinterpret_cast<void***>(device);

	Hook(vtable, kCreateTextureIndex, g_createTextureHook, &HookedCreateTexture);
	Hook(vtable, kCreateRenderTargetIndex, g_createRenderTargetHook, &HookedCreateRenderTarget);
	Hook(vtable, kCreateDepthStencilSurfaceIndex, g_createDepthStencilHook, &HookedCreateDepthStencilSurface);
	Hook(vtable, kSetRenderTargetIndex, g_setRenderTargetHook, &HookedSetRenderTarget);

	ScaledDeviceState::Attach(device);

	LOG("[InternalResolution] the scene targets will be created at %ux%u", g_width, g_height);
}

void ScaledTargets::OnDeviceLost()
{
	ExclusiveLock lock;
	g_targetCount = 0;
	g_boundScaled = false;
}

bool ScaledTargets::IsActive()
{
	return g_active;
}

bool ScaledTargets::NeedsRestart()
{
	if (!g_attached)
		return false;

	unsigned width = 0;
	unsigned height = 0;
	const bool wanted = WantedSize(width, height);

	return wanted != g_active || (wanted && (width != g_width || height != g_height));
}

bool ScaledTargets::IsReduced()
{
	return g_active && g_width < InternalResolution::kBaseWidth;
}

unsigned ScaledTargets::Width()
{
	return g_width;
}

unsigned ScaledTargets::Height()
{
	return g_height;
}

float ScaledTargets::ScaleX()
{
	return g_scaleX;
}

float ScaledTargets::ScaleY()
{
	return g_scaleY;
}

bool ScaledTargets::IsRegistered(const IDirect3DSurface9* surface)
{
	if (surface == nullptr || g_targetCount == 0)
		return false;

	SharedLock lock;

	for (int i = 0; i < g_targetCount; ++i)
	{
		if (g_targets[i].surface == surface)
			return true;
	}

	return false;
}

bool ScaledTargets::IsScaled(IDirect3DSurface9* surface)
{
	if (!IsRegistered(surface))
		return false;

	D3DSURFACE_DESC desc = {};
	return TargetSizeMask::RealDesc(surface, desc) && desc.Width == g_width && desc.Height == g_height;
}

bool ScaledTargets::IsScaledTexture(const IDirect3DBaseTexture9* texture)
{
	if (texture == nullptr || g_targetCount == 0)
		return false;

	SharedLock lock;

	for (int i = 0; i < g_targetCount; ++i)
	{
		if (g_targets[i].texture == texture)
			return true;
	}

	return false;
}

bool ScaledTargets::BoundIsScaled()
{
	return g_boundScaled;
}

int ScaledTargets::CollectSurfaces(IDirect3DSurface9** out, int capacity)
{
	SharedLock lock;
	int count = 0;

	for (int i = 0; i < g_targetCount && count < capacity; ++i)
		out[count++] = static_cast<IDirect3DSurface9*>(const_cast<void*>(g_targets[i].surface));

	return count;
}

LONG ScaledTargets::ToScaledX(LONG value)
{
	return Scale(value, g_scaleX);
}

LONG ScaledTargets::ToScaledY(LONG value)
{
	return Scale(value, g_scaleY);
}

LONG ScaledTargets::ToBaseX(LONG value)
{
	return Scale(value, 1.0f / g_scaleX);
}

LONG ScaledTargets::ToBaseY(LONG value)
{
	return Scale(value, 1.0f / g_scaleY);
}

const char* ScaledTargets::GetStatusText()
{
	if (!g_active)
		return g_status;

	snprintf(g_status, sizeof(g_status), "%ux%u, %d scene targets scaled, %d alive, %d refused by the device",
		g_width, g_height, g_scaledCreations, g_targetCount, g_refusedCreations);
	return g_status;
}
