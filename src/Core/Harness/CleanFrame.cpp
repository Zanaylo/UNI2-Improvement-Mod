#include "Core/Harness/CleanFrame.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Menus/BattleCockpit.h"

#include <d3d9.h>

#include <cstdint>

namespace {

constexpr unsigned kStages = 8;
constexpr UINT kShadowSide = 1024;

volatile bool g_on = false;
uint32_t g_paletteStages = 0;
IDirect3DSurface9* g_stageTarget = nullptr;
uint32_t g_inputDisplay = 0;

bool IsShadowSized(IDirect3DSurface9* surface)
{
	D3DSURFACE_DESC desc = {};

	return SUCCEEDED(surface->GetDesc(&desc)) && desc.Width == kShadowSide &&
		desc.Height == kShadowSide && (desc.Usage & D3DUSAGE_RENDERTARGET) != 0;
}

bool SamplesShadow(IDirect3DDevice9* device)
{
	IDirect3DBaseTexture9* texture = nullptr;

	if (FAILED(device->GetTexture(0, &texture)) || texture == nullptr)
		return false;

	bool shadow = false;

	if (texture->GetType() == D3DRTYPE_TEXTURE)
	{
		IDirect3DTexture9* const flat = static_cast<IDirect3DTexture9*>(texture);
		IDirect3DSurface9* level = nullptr;

		if (SUCCEEDED(flat->GetSurfaceLevel(0, &level)) && level != nullptr)
		{
			shadow = IsShadowSized(level);
			level->Release();
		}
	}

	texture->Release();
	return shadow;
}

bool HasVertexShader(IDirect3DDevice9* device)
{
	IDirect3DVertexShader9* shader = nullptr;

	if (FAILED(device->GetVertexShader(&shader)) || shader == nullptr)
		return false;

	shader->Release();
	return true;
}

bool Skips(IDirect3DDevice9* device, IDirect3DSurface9* target)
{
	if (IsShadowSized(target) || SamplesShadow(device))
		return true;

	if (!HasVertexShader(device))
		return false;

	if (g_stageTarget == nullptr)
	{
		g_stageTarget = target;
		return false;
	}

	return target != g_stageTarget;
}

void HideInputDisplay(bool hidden)
{
	void* const option = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kInputDisplayOption));

	if (!hidden)
	{
		TryWriteDword(option, g_inputDisplay);
		return;
	}

	TryReadDword(option, g_inputDisplay);
	TryWriteDword(option, 0);
}

}

void CleanFrame::SetOn(bool on)
{
	if (on == g_on)
		return;

	HideInputDisplay(on);
	g_on = on;
	BattleCockpit::SetHidden(on, false);
}

bool CleanFrame::IsOn()
{
	return g_on;
}

void CleanFrame::OnSetTexture(unsigned stage, bool paletteShaped)
{
	if (stage >= kStages)
		return;

	const uint32_t bit = 1u << stage;
	g_paletteStages = paletteShaped ? (g_paletteStages | bit) : (g_paletteStages & ~bit);
}

void CleanFrame::OnPresent()
{
	g_stageTarget = nullptr;
}

bool CleanFrame::SkipsDraw(IDirect3DDevice9* device)
{
	if (!g_on)
		return false;

	if (g_paletteStages != 0)
		return true;

	IDirect3DSurface9* target = nullptr;

	if (FAILED(device->GetRenderTarget(0, &target)) || target == nullptr)
		return false;

	const bool skip = Skips(device, target);
	target->Release();

	return skip;
}
