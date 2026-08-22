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
	
		return parameters.BackBufferCount;

	return 2;
}


void RewriteMultiSample(D3DPRESENT_PARAMETERS& parameters)
{
	if (!g_modVals.disableBackBufferAa || parameters.MultiSampleType == D3DMULTISAMPLE_NONE)
		return;

	LOG("[PresentTuning] back buffer multisampling %u -> none",
		static_cast<unsigned>(parameters.MultiSampleType));

	parameters.MultiSampleType = D3DMULTISAMPLE_NONE;
	parameters.MultiSampleQuality = 0;
}


void RewriteBackBufferSize(D3DPRESENT_PARAMETERS& parameters)
{
	if (g_modVals.presentWidth <= 0 || g_modVals.presentHeight <= 0 || !parameters.Windowed)
		return;

	const UINT width = static_cast<UINT>(g_modVals.presentWidth);
	const UINT height = static_cast<UINT>(g_modVals.presentHeight);

	if (width == parameters.BackBufferWidth && height == parameters.BackBufferHeight)
		return;

	LOG("[PresentTuning] back buffer %ux%u -> %ux%u", parameters.BackBufferWidth,
		parameters.BackBufferHeight, width, height);

	parameters.BackBufferWidth = width;
	parameters.BackBufferHeight = height;
}

void Rewrite(IDirect3D9* d3d9, UINT adapter, D3DPRESENT_PARAMETERS& parameters)
{
	RewriteMultiSample(parameters);
	RewriteBackBufferSize(parameters);

	if (parameters.Windowed)
	{
		Decide("windowed - back buffer %ux%u, the compositor owns the rest",
			parameters.BackBufferWidth, parameters.BackBufferHeight);
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
		RewriteBackBufferSize(parameters);
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
		RewriteBackBufferSize(parameters);
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
