#include "Core/Harness/CleanFrame.h"

#include "Core/Harness/CleanFrameRule.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Device/SampledStages.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Menus/BattleCockpit.h"

#include <d3d9.h>

#include <cstdint>
#include <cstring>

namespace {

using CleanFrameRule::Verdict;

constexpr unsigned kStages = 8;
constexpr UINT kShadowSide = 1024;
constexpr size_t kVerdicts = static_cast<size_t>(Verdict::Count);

volatile bool g_on = false;
volatile bool g_fightersHidden = false;
bool g_hidingFighters = false;
uint32_t g_paletteStages = 0;
IDirect3DSurface9* g_stageTarget = nullptr;
IDirect3DSurface9* g_backBuffer = nullptr;
bool g_backBufferKnown = false;
uint32_t g_inputDisplay = 0;
int g_verdicts[kVerdicts] = {};
bool g_wasFiltering = false;

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

IDirect3DSurface9* BackBuffer(IDirect3DDevice9* device)
{
	if (g_backBufferKnown)
		return g_backBuffer;

	g_backBufferKnown = true;
	g_backBuffer = nullptr;

	IDirect3DSurface9* backBuffer = nullptr;

	if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || backBuffer == nullptr)
		return nullptr;

	backBuffer->Release();
	g_backBuffer = backBuffer;
	return g_backBuffer;
}

bool SamplesPalette(IDirect3DDevice9* device)
{
	return g_paletteStages != 0 && (g_paletteStages & SampledStages::Of(device, kStages)) != 0;
}

bool OnStageTarget(IDirect3DSurface9* target)
{
	if (g_stageTarget == nullptr)
		g_stageTarget = target;

	return target == g_stageTarget;
}

CleanFrameRule::Draw Describe(IDirect3DDevice9* device, IDirect3DSurface9* target)
{
	CleanFrameRule::Draw draw = {};
	draw.onScreen = target == BackBuffer(device);
	draw.paletteBound = SamplesPalette(device);

	if (draw.paletteBound)
		return draw;

	draw.shadow = IsShadowSized(target) || SamplesShadow(device);

	if (draw.shadow)
		return draw;

	draw.shaded = HasVertexShader(device);
	draw.onStageTarget = draw.shaded && OnStageTarget(target);
	return draw;
}

int& Counted(Verdict verdict)
{
	return g_verdicts[static_cast<size_t>(verdict)];
}

void SizeOf(IDirect3DSurface9* surface, UINT& width, UINT& height)
{
	D3DSURFACE_DESC desc = {};

	if (surface == nullptr || FAILED(surface->GetDesc(&desc)))
		return;

	width = desc.Width;
	height = desc.Height;
}

void ReportFirstFilteredFrame()
{
	UINT stageWidth = 0;
	UINT stageHeight = 0;
	UINT screenWidth = 0;
	UINT screenHeight = 0;
	SizeOf(g_stageTarget, stageWidth, stageHeight);
	SizeOf(g_backBuffer, screenWidth, screenHeight);

	LOG("CleanFrame: first filtered frame kept %d draw(s), dropped %d with a palette, %d by the shadow and %d off "
		"the stage target (%ux%u); kept %d into the back buffer (%ux%u) that would have been dropped",
		Counted(Verdict::Kept), Counted(Verdict::Palette), Counted(Verdict::Shadow), Counted(Verdict::OffStage),
		stageWidth, stageHeight, Counted(Verdict::KeptOnScreen), screenWidth, screenHeight);
}

void CloseFrame(bool filtered)
{
	if (filtered && !g_wasFiltering)
		ReportFirstFilteredFrame();

	g_wasFiltering = filtered;
	memset(g_verdicts, 0, sizeof(g_verdicts));
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

void CleanFrame::SetFightersHidden(bool hidden)
{
	g_fightersHidden = hidden;
}

bool CleanFrame::FightersHidden()
{
	return g_fightersHidden;
}

void CleanFrame::OnSetTexture(unsigned stage, bool paletteShaped)
{
	if (stage >= kStages)
		return;

	const uint32_t bit = 1u << stage;
	g_paletteStages = paletteShaped ? (g_paletteStages | bit) : (g_paletteStages & ~bit);
}

void CleanFrame::OnPresentBegin()
{
	CloseFrame(g_on || g_hidingFighters);
	g_hidingFighters = false;
}

void CleanFrame::OnPresent()
{
	g_stageTarget = nullptr;
	g_backBufferKnown = false;
	g_hidingFighters = g_fightersHidden && GameState::AllowsTrainingTools();
}

bool CleanFrame::SkipsDraw(IDirect3DDevice9* device)
{
	if (!g_on && !g_hidingFighters)
		return false;

	IDirect3DSurface9* target = nullptr;

	if (FAILED(device->GetRenderTarget(0, &target)) || target == nullptr)
		return false;

	const Verdict verdict = CleanFrameRule::Judge(Describe(device, target));
	target->Release();

	++Counted(verdict);
	return CleanFrameRule::Drops(verdict);
}
