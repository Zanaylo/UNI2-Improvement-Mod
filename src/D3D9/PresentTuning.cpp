#include "D3D9/PresentTuning.h"

#include "Core/Compat.h"
#include "Core/interfaces.h"
#include "Core/logger.h"

#include <cstdarg>
#include <cstdio>

namespace {

constexpr UINT kTickRateHz = 60;

char g_decision[256] = "not applied yet";

void Decide(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(g_decision, sizeof(g_decision), format, args);
	va_end(args);
}

bool VsyncIsOn(const D3DPRESENT_PARAMETERS& parameters)
{
	return parameters.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE;
}

bool RateIsListed(IDirect3D9* d3d9, UINT adapter, const D3DPRESENT_PARAMETERS& parameters, UINT rate)
{
	if (d3d9 == nullptr || parameters.BackBufferFormat == D3DFMT_UNKNOWN)
		return false;

	const UINT modeCount = d3d9->GetAdapterModeCount(adapter, parameters.BackBufferFormat);

	for (UINT i = 0; i < modeCount; ++i)
	{
		D3DDISPLAYMODE mode = {};
		if (FAILED(d3d9->EnumAdapterModes(adapter, parameters.BackBufferFormat, i, &mode)))
			continue;

		if (mode.Width == parameters.BackBufferWidth && mode.Height == parameters.BackBufferHeight &&
			mode.RefreshRate == rate)
		{
			return true;
		}
	}

	return false;
}

// The highest rate the adapter lists at the game resolution that divides evenly by 60 and is not
// above the desktop one. Used only when vsync is on and the desktop rate is not itself a multiple
// of 60, because that is the one case where the display grid and the 16.667 ms deadline beat.
UINT HighestMultipleOfSixty(IDirect3D9* d3d9, UINT adapter,
	const D3DPRESENT_PARAMETERS& parameters, UINT ceiling)
{
	if (d3d9 == nullptr || parameters.BackBufferFormat == D3DFMT_UNKNOWN)
		return 0;

	const UINT modeCount = d3d9->GetAdapterModeCount(adapter, parameters.BackBufferFormat);
	UINT best = 0;

	for (UINT i = 0; i < modeCount; ++i)
	{
		D3DDISPLAYMODE mode = {};
		if (FAILED(d3d9->EnumAdapterModes(adapter, parameters.BackBufferFormat, i, &mode)))
			continue;

		if (mode.Width != parameters.BackBufferWidth || mode.Height != parameters.BackBufferHeight)
			continue;

		if (mode.RefreshRate == 0 || mode.RefreshRate % kTickRateHz != 0)
			continue;

		if (ceiling != 0 && mode.RefreshRate > ceiling)
			continue;

		if (mode.RefreshRate > best)
			best = mode.RefreshRate;
	}

	return best;
}

UINT ChooseRefreshRate(IDirect3D9* d3d9, UINT adapter, const D3DPRESENT_PARAMETERS& parameters)
{
	if (g_modVals.fullscreenRefreshHz > 0)
		return static_cast<UINT>(g_modVals.fullscreenRefreshHz);

	if (d3d9 == nullptr)
		return parameters.FullScreen_RefreshRateInHz;

	D3DDISPLAYMODE desktop = {};
	if (FAILED(d3d9->GetAdapterDisplayMode(adapter, &desktop)) || desktop.RefreshRate == 0)
		return parameters.FullScreen_RefreshRateInHz;

	// With vsync off the engine limiter is the only pacer and Present never blocks, so the refresh
	// rate decides nothing but how long a frame takes to scan out. Leave the desktop mode alone: a
	// switch costs an alt-tab and a downshift to 60 costs scanout for nothing.
	if (!VsyncIsOn(parameters))
	{
		if (RateIsListed(d3d9, adapter, parameters, desktop.RefreshRate))
			return desktop.RefreshRate;

		return parameters.FullScreen_RefreshRateInHz;
	}

	if (desktop.RefreshRate % kTickRateHz == 0 &&
		RateIsListed(d3d9, adapter, parameters, desktop.RefreshRate))
	{
		return desktop.RefreshRate;
	}

	const UINT even = HighestMultipleOfSixty(d3d9, adapter, parameters, desktop.RefreshRate);
	if (even != 0)
		return even;

	return parameters.FullScreen_RefreshRateInHz;
}

UINT ChooseBackBufferCount(const D3DPRESENT_PARAMETERS& parameters)
{
	// A second buffer can only absorb a missed deadline where there is a deadline to miss. With the
	// present interval immediate there is none, and the buffer becomes a queued frame of latency.
	if (!g_modVals.extraBackBuffer || !VsyncIsOn(parameters))
		return parameters.BackBufferCount;

	return 2;
}

// A Direct3D 9 texture cannot be multisampled, and the game renders its whole scene into textures.
// The only thing the multisampled back buffer ever antialiases is the edge of the single quad the
// finished frame is drawn with, which is the edge of the screen.
void RewriteMultiSample(D3DPRESENT_PARAMETERS& parameters)
{
	if (!g_modVals.disableBackBufferAa || parameters.MultiSampleType == D3DMULTISAMPLE_NONE)
		return;

	LOG("[PresentTuning] back buffer multisampling %u -> none",
		static_cast<unsigned>(parameters.MultiSampleType));

	parameters.MultiSampleType = D3DMULTISAMPLE_NONE;
	parameters.MultiSampleQuality = 0;
}

void Rewrite(IDirect3D9* d3d9, UINT adapter, D3DPRESENT_PARAMETERS& parameters)
{
	RewriteMultiSample(parameters);

	if (parameters.Windowed)
	{
		Decide("windowed - nothing rewritten, the compositor owns the presentation");
		return;
	}

	const UINT refreshRate = ChooseRefreshRate(d3d9, adapter, parameters);
	const UINT backBufferCount = ChooseBackBufferCount(parameters);

	Decide("fullscreen, vsync %s: refresh %u -> %u, buffers %u -> %u",
		VsyncIsOn(parameters) ? "on" : "off", parameters.FullScreen_RefreshRateInHz, refreshRate,
		parameters.BackBufferCount, backBufferCount);

	if (refreshRate == parameters.FullScreen_RefreshRateInHz &&
		backBufferCount == parameters.BackBufferCount)
	{
		return;
	}

	LOG("[PresentTuning] %s", g_decision);

	parameters.FullScreen_RefreshRateInHz = refreshRate;
	parameters.BackBufferCount = backBufferCount;
}

}

void PresentTuning::Apply(IDirect3D9* d3d9, UINT adapter, D3DPRESENT_PARAMETERS& parameters)
{
	if (!g_modVals.displayTuning || Compat::SafeMode())
	{
		Decide(Compat::SafeMode() ? "safe mode - the host owns the presentation"
			: "off - the game own parameters");
		RewriteMultiSample(parameters);
		return;
	}

	Rewrite(d3d9, adapter, parameters);
}

void PresentTuning::Apply(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS& parameters)
{
	if (device == nullptr)
		return;

	if (!g_modVals.displayTuning || Compat::SafeMode())
	{
		RewriteMultiSample(parameters);
		return;
	}

	IDirect3D9* d3d9 = nullptr;
	if (FAILED(device->GetDirect3D(&d3d9)) || d3d9 == nullptr)
	{
		Rewrite(nullptr, D3DADAPTER_DEFAULT, parameters);
		return;
	}

	D3DDEVICE_CREATION_PARAMETERS creation = {};
	const UINT adapter = SUCCEEDED(device->GetCreationParameters(&creation)) ? creation.AdapterOrdinal
		: D3DADAPTER_DEFAULT;

	Rewrite(d3d9, adapter, parameters);
	d3d9->Release();
}

const char* PresentTuning::GetLastDecision()
{
	return g_decision;
}
