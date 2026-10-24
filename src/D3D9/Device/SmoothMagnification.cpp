#include "D3D9/Device/SmoothMagnification.h"

#include "Core/logger.h"
#include "D3D9/Device/MagnifyRule.h"
#include "D3D9/Device/ScaledTargets.h"

namespace {

constexpr DWORD kStage = 0;
constexpr int kMostLoggedFormats = 16;

D3DFORMAT g_logged[kMostLoggedFormats] = {};
int g_loggedCount = 0;

void NoteFormat(D3DFORMAT format, bool smoothed)
{
	for (int i = 0; i < g_loggedCount; ++i)
	{
		if (g_logged[i] == format)
			return;
	}

	if (g_loggedCount == kMostLoggedFormats)
		return;

	g_logged[g_loggedCount++] = format;
	LOG("[InternalResolution] screen space art in format %d is %s", static_cast<int>(format),
		smoothed ? "magnified with a linear filter" : "left as the game samples it");
}

D3DFORMAT FormatOf(IDirect3DBaseTexture9* texture)
{
	if (texture->GetType() != D3DRTYPE_TEXTURE)
		return D3DFMT_UNKNOWN;

	D3DSURFACE_DESC desc = {};

	if (FAILED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0, &desc)))
		return D3DFMT_UNKNOWN;

	return desc.Format;
}

}

SmoothMagnification::SmoothMagnification(IDirect3DDevice9* device)
	: m_device(device)
{
	IDirect3DBaseTexture9* texture = nullptr;

	if (FAILED(device->GetTexture(kStage, &texture)) || texture == nullptr)
		return;

	const D3DFORMAT format = FormatOf(texture);
	const bool scaled = ScaledTargets::IsScaledTexture(texture);
	texture->Release();

	DWORD filter = D3DTEXF_NONE;
	device->GetSamplerState(kStage, D3DSAMP_MAGFILTER, &filter);

	const bool smooth = MagnifyRule::WantsLinear(format, filter, scaled);
	NoteFormat(format, smooth);

	if (!smooth)
		return;

	m_changed = SUCCEEDED(device->SetSamplerState(kStage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
}

SmoothMagnification::~SmoothMagnification()
{
	if (m_changed)
		m_device->SetSamplerState(kStage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
}
