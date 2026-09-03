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

template <typename Visit>
void ForEachMode(IDirect3D9* d3d9, UINT adapter, D3DFORMAT format, Visit visit)
{
	if (d3d9 == nullptr || format == D3DFMT_UNKNOWN)
		return;

	const UINT modeCount = d3d9->GetAdapterModeCount(adapter, format);

	for (UINT i = 0; i < modeCount; ++i)
	{
		D3DDISPLAYMODE mode = {};

		if (SUCCEEDED(d3d9->EnumAdapterModes(adapter, format, i, &mode)))
			visit(mode);
	}
}

bool RateIsListed(IDirect3D9* d3d9, UINT adapter, const D3DPRESENT_PARAMETERS& parameters, UINT rate)
{
	bool listed = false;

	ForEachMode(d3d9, adapter, parameters.BackBufferFormat, [&](const D3DDISPLAYMODE& mode)
	{
		if (mode.Width == parameters.BackBufferWidth && mode.Height == parameters.BackBufferHeight &&
			mode.RefreshRate == rate)
		{
			listed = true;
		}
	});

	return listed;
}


UINT HighestMultipleOfSixty(IDirect3D9* d3d9, UINT adapter,
	const D3DPRESENT_PARAMETERS& parameters, UINT ceiling)
{
	UINT best = 0;

	ForEachMode(d3d9, adapter, parameters.BackBufferFormat, [&](const D3DDISPLAYMODE& mode)
	{
		if (mode.Width != parameters.BackBufferWidth || mode.Height != parameters.BackBufferHeight)
			return;

		if (mode.RefreshRate == 0 || mode.RefreshRate % kTickRateHz != 0)
			return;

		if (ceiling != 0 && mode.RefreshRate > ceiling)
			return;

		if (mode.RefreshRate > best)
			best = mode.RefreshRate;
	});

	return best;
}

bool SmallestListedAtLeast(IDirect3D9* d3d9, UINT adapter, D3DFORMAT format, UINT& width,
	UINT& height)
{
	const UINT wantWidth = width;
	const UINT wantHeight = height;

	UINT bestPixels = 0;

	ForEachMode(d3d9, adapter, format, [&](const D3DDISPLAYMODE& mode)
	{
		if (mode.Width < wantWidth || mode.Height < wantHeight)
			return;

		const UINT pixels = mode.Width * mode.Height;

		if (bestPixels != 0 && pixels >= bestPixels)
			return;

		bestPixels = pixels;
		width = mode.Width;
		height = mode.Height;
	});

	return bestPixels != 0;
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
	if (!g_modVals.extraBackBuffer || !VsyncIsOn(parameters))
		return parameters.BackBufferCount;

	return parameters.BackBufferCount >= 2 ? parameters.BackBufferCount : 2;
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


void RewriteBackBufferSize(IDirect3D9* d3d9, UINT adapter, D3DPRESENT_PARAMETERS& parameters)
{
	if (g_modVals.presentWidth <= 0 || g_modVals.presentHeight <= 0)
		return;

	UINT width = static_cast<UINT>(g_modVals.presentWidth);
	UINT height = static_cast<UINT>(g_modVals.presentHeight);

	if (!parameters.Windowed)
	{
		if (width >= parameters.BackBufferWidth && height >= parameters.BackBufferHeight)
			return;

		const UINT askedWidth = width;
		const UINT askedHeight = height;

		if (!SmallestListedAtLeast(d3d9, adapter, parameters.BackBufferFormat, width, height))
		{
			LOG("[PresentTuning] %ux%u is not a mode this adapter lists and nor is anything "
				"larger, so the drawing size is left alone", askedWidth, askedHeight);
			return;
		}

		if (width != askedWidth || height != askedHeight)
		{
			LOG("[PresentTuning] %ux%u is not a mode this adapter lists, using %ux%u",
				askedWidth, askedHeight, width, height);
		}
	}

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
	RewriteBackBufferSize(d3d9, adapter, parameters);

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
		RewriteBackBufferSize(d3d9, adapter, parameters);
		return;
	}

	Rewrite(d3d9, adapter, parameters);
}

void PresentTuning::Apply(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS& parameters)
{
	if (device == nullptr)
		return;

	IDirect3D9* d3d9 = nullptr;
	UINT adapter = D3DADAPTER_DEFAULT;

	if (FAILED(device->GetDirect3D(&d3d9)))
		d3d9 = nullptr;

	if (d3d9 != nullptr)
	{
		D3DDEVICE_CREATION_PARAMETERS creation = {};

		if (SUCCEEDED(device->GetCreationParameters(&creation)))
			adapter = creation.AdapterOrdinal;
	}

	if (!g_modVals.displayTuning || Compat::SafeMode())
	{
		RewriteMultiSample(parameters);
		RewriteBackBufferSize(d3d9, adapter, parameters);
	}
	else
	{
		Rewrite(d3d9, adapter, parameters);
	}

	if (d3d9 != nullptr)
		d3d9->Release();
}

const char* PresentTuning::GetLastDecision()
{
	return g_decision;
}
