#include "D3D9/Device/TargetSizeMask.h"

#include "Core/logger.h"
#include "D3D9/Device/GameCaller.h"
#include "D3D9/Device/ScaledTargets.h"
#include "Game/Display/InternalResolution.h"
#include "Hooks/GameHook.h"

namespace {

constexpr int kTextureGetLevelDescIndex = 17;
constexpr int kSurfaceGetDescIndex = 12;
constexpr int kMaxImplementations = 3;

using GetLevelDesc_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*, UINT, D3DSURFACE_DESC*);
using GetDesc_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DSurface9*, D3DSURFACE_DESC*);

GameHook<GetLevelDesc_t> g_levelDescHooks[kMaxImplementations] = {
	GameHook<GetLevelDesc_t>("IDirect3DTexture9::GetLevelDesc"),
	GameHook<GetLevelDesc_t>("IDirect3DTexture9::GetLevelDesc"),
	GameHook<GetLevelDesc_t>("IDirect3DTexture9::GetLevelDesc"),
};

GameHook<GetDesc_t> g_descHooks[kMaxImplementations] = {
	GameHook<GetDesc_t>("IDirect3DSurface9::GetDesc"),
	GameHook<GetDesc_t>("IDirect3DSurface9::GetDesc"),
	GameHook<GetDesc_t>("IDirect3DSurface9::GetDesc"),
};

bool IsScaledSize(const D3DSURFACE_DESC& desc)
{
	return desc.Width == ScaledTargets::Width() && desc.Height == ScaledTargets::Height();
}

void ShowBaseSize(D3DSURFACE_DESC& desc)
{
	desc.Width = InternalResolution::kBaseWidth;
	desc.Height = InternalResolution::kBaseHeight;
}

template <int Slot>
HRESULT STDMETHODCALLTYPE MaskedGetLevelDesc(IDirect3DTexture9* texture, UINT level, D3DSURFACE_DESC* desc)
{
	const HRESULT result = g_levelDescHooks[Slot].Original()(texture, level, desc);

	if (SUCCEEDED(result) && desc != nullptr && level == 0 && IsScaledSize(*desc) && IS_GAME_CALLER() &&
		ScaledTargets::IsScaledTexture(texture))
	{
		ShowBaseSize(*desc);
	}

	return result;
}

template <int Slot>
HRESULT STDMETHODCALLTYPE MaskedGetDesc(IDirect3DSurface9* surface, D3DSURFACE_DESC* desc)
{
	const HRESULT result = g_descHooks[Slot].Original()(surface, desc);

	if (SUCCEEDED(result) && desc != nullptr && IsScaledSize(*desc) && IS_GAME_CALLER() &&
		ScaledTargets::IsRegistered(surface))
		ShowBaseSize(*desc);

	return result;
}

constexpr GetLevelDesc_t kLevelDescDetours[kMaxImplementations] = {
	&MaskedGetLevelDesc<0>, &MaskedGetLevelDesc<1>, &MaskedGetLevelDesc<2>,
};

constexpr GetDesc_t kDescDetours[kMaxImplementations] = {
	&MaskedGetDesc<0>, &MaskedGetDesc<1>, &MaskedGetDesc<2>,
};

void* VTableEntry(const void* object, int index)
{
	void** const vtable = *reinterpret_cast<void** const*>(object);
	return vtable[index];
}

template <typename Fn>
int SlotOf(GameHook<Fn> (&hooks)[kMaxImplementations], const void* target)
{
	for (int i = 0; i < kMaxImplementations; ++i)
	{
		if (hooks[i].IsLive() && hooks[i].Target() == target)
			return i;
	}

	return -1;
}

template <typename Fn>
void Cover(GameHook<Fn> (&hooks)[kMaxImplementations], const Fn (&detours)[kMaxImplementations],
	void* target)
{
	if (target == nullptr || SlotOf(hooks, target) >= 0)
		return;

	for (int i = 0; i < kMaxImplementations; ++i)
	{
		if (hooks[i].IsLive())
			continue;

		if (!hooks[i].Install(target, detours[i]))
			LOG("[InternalResolution] could not hook %s at 0x%p", hooks[i].Label(), target);

		return;
	}

	LOG("[InternalResolution] more than %d %s implementations, 0x%p reports its real size",
		kMaxImplementations, hooks[0].Label(), target);
}

}

void TargetSizeMask::Watch(IDirect3DTexture9* texture)
{
	if (texture != nullptr)
		Cover(g_levelDescHooks, kLevelDescDetours, VTableEntry(texture, kTextureGetLevelDescIndex));
}

void TargetSizeMask::Watch(IDirect3DSurface9* surface)
{
	if (surface != nullptr)
		Cover(g_descHooks, kDescDetours, VTableEntry(surface, kSurfaceGetDescIndex));
}

bool TargetSizeMask::RealDesc(IDirect3DSurface9* surface, D3DSURFACE_DESC& out)
{
	return surface != nullptr && SUCCEEDED(surface->GetDesc(&out));
}
