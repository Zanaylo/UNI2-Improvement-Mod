#include "D3D9/Post/PostChain.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "D3D9/Post/Shaders/BloomBlurShader.h"
#include "D3D9/Post/Shaders/BloomBrightShader.h"
#include "D3D9/Post/Shaders/BloomMixShader.h"
#include "D3D9/Post/DeviceState.h"
#include "D3D9/Post/FullScreenPass.h"
#include "D3D9/Post/Shaders/FxaaShader.h"
#include "D3D9/Post/Shaders/LookShader.h"
#include "D3D9/Post/PostOptions.h"
#include "D3D9/Post/ScratchTarget.h"
#include "D3D9/Post/ShaderPack.h"

#include <cstdarg>
#include <cstdio>

namespace {

enum Stage
{
	Stage_Fxaa,
	Stage_Bloom,
	Stage_Look,
	Stage_Sharpen,
	Stage_Pack,
	Stage_COUNT
};

constexpr unsigned kBloomDivisor = 4;
constexpr unsigned kBloomMinimum = 32;

PixelShaderHandle g_fxaa;
PixelShaderHandle g_sharpen;
PixelShaderHandle g_look;
PixelShaderHandle g_bloomBright;
PixelShaderHandle g_bloomBlur;
PixelShaderHandle g_bloomMix;

ScratchTarget g_front;
ScratchTarget g_back;
ScratchTarget g_bloomFront;
ScratchTarget g_bloomBack;
DeviceState g_state;

bool g_failed = false;
unsigned long g_frames = 0;

char g_status[224] = "off";

void Report(const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(g_status, sizeof(g_status), format, arguments);
	va_end(arguments);
}

float Signed(int value)
{
	return static_cast<float>(value) / 100.0f;
}

float Positive(int value)
{
	if (value <= 0)
		return 0.0f;

	return value >= 100 ? 1.0f : static_cast<float>(value) / 100.0f;
}

bool LookIsOn()
{
	return g_modVals.lookEnabled && !PostChain::IsLookNeutral();
}

int CollectStages(int* out)
{
	int count = 0;

	if (AntiAlias::Clamp(g_modVals.antiAliasing) != AntiAlias::Level_Off)
		out[count++] = Stage_Fxaa;

	if (g_modVals.bloomEnabled && g_modVals.bloomIntensity > 0)
		out[count++] = Stage_Bloom;

	if (LookIsOn())
		out[count++] = Stage_Look;

	if (SharpenMode::Clamp(g_modVals.sharpenMode) != SharpenMode::Kind_Off &&
		g_modVals.sharpenStrength > 0)
	{
		out[count++] = Stage_Sharpen;
	}

	if (ShaderPack::GetSelected() >= 0)
		out[count++] = Stage_Pack;

	return count;
}

bool EnsureBloomShaders(IDirect3DDevice9* device)
{
	return g_bloomBright.Ensure(device, kBloomBrightShader) &&
		g_bloomBlur.Ensure(device, kBloomBlurShader) &&
		g_bloomMix.Ensure(device, kBloomMixShader);
}

IDirect3DPixelShader9* ShaderFor(IDirect3DDevice9* device, int stage)
{
	switch (stage)
	{
	case Stage_Fxaa:
		return g_fxaa.Ensure(device, kFxaaShader) ? g_fxaa.Get() : nullptr;

	case Stage_Bloom:
		return EnsureBloomShaders(device) ? g_bloomMix.Get() : nullptr;

	case Stage_Look:
		return g_look.Ensure(device, kLookShader) ? g_look.Get() : nullptr;

	case Stage_Sharpen:
		return g_sharpen.Ensure(device, SharpenMode::GetBytecode(g_modVals.sharpenMode))
			? g_sharpen.Get() : nullptr;

	case Stage_Pack:
		return ShaderPack::Acquire(device);

	default:
		return nullptr;
	}
}

bool StageWantsLinear(int stage)
{
	return stage == Stage_Fxaa;
}

void SetStageConstants(IDirect3DDevice9* device, int stage, unsigned width, unsigned height)
{
	const float texelX = 1.0f / static_cast<float>(width);
	const float texelY = 1.0f / static_cast<float>(height);

	switch (stage)
	{
	case Stage_Fxaa:
	{
		const AntiAlias::Tuning tuning = AntiAlias::GetTuning(g_modVals.antiAliasing);

		FullScreenQuad::SetConstant(device, 0, texelX, texelY, static_cast<float>(width),
			static_cast<float>(height));
		FullScreenQuad::SetConstant(device, 1, tuning.edgeThreshold, tuning.edgeThresholdMin,
			tuning.subpixel, tuning.steps);
		return;
	}

	case Stage_Sharpen:
		FullScreenQuad::SetConstant(device, 0, texelX, texelY, 0.0f, 0.0f);
		FullScreenQuad::SetConstant(device, 1, Positive(g_modVals.sharpenStrength), 0.0f, 0.0f,
			0.0f);
		return;

	case Stage_Look:
		FullScreenQuad::SetConstant(device, 0, Signed(g_modVals.lookBrightness),
			Signed(g_modVals.lookContrast), static_cast<float>(g_modVals.lookGamma) / 100.0f,
			Signed(g_modVals.lookSaturation));
		FullScreenQuad::SetConstant(device, 1, Signed(g_modVals.lookVibrance),
			Signed(g_modVals.lookTemperature), Positive(g_modVals.lookVignette),
			Positive(g_modVals.lookScanlines));
		FullScreenQuad::SetConstant(device, 2, static_cast<float>(width),
			static_cast<float>(height), g_modVals.lookDither ? 1.0f : 0.0f, 0.0f);
		return;

	case Stage_Pack:
		FullScreenQuad::SetConstant(device, 0, texelX, texelY, static_cast<float>(width),
			static_cast<float>(height));
		FullScreenQuad::SetConstant(device, 1, static_cast<float>(g_frames) / 60.0f,
			static_cast<float>(g_frames), 0.0f, 0.0f);
		return;

	default:
		return;
	}
}

void BloomSize(const D3DSURFACE_DESC& desc, unsigned& outWidth, unsigned& outHeight)
{
	outWidth = desc.Width / kBloomDivisor;
	outHeight = desc.Height / kBloomDivisor;

	if (outWidth < kBloomMinimum)
		outWidth = kBloomMinimum;

	if (outHeight < kBloomMinimum)
		outHeight = kBloomMinimum;
}

bool EnsureBloomTargets(IDirect3DDevice9* device, const D3DSURFACE_DESC& desc)
{
	unsigned width = 0;
	unsigned height = 0;
	BloomSize(desc, width, height);

	return g_bloomFront.Ensure(device, width, height, desc.Format) &&
		g_bloomBack.Ensure(device, width, height, desc.Format);
}

void RunBloom(IDirect3DDevice9* device, ScratchTarget& source, IDirect3DSurface9* destination,
	const D3DSURFACE_DESC& desc)
{
	const unsigned width = g_bloomFront.Width();
	const unsigned height = g_bloomFront.Height();

	const float texelX = 1.0f / static_cast<float>(width);
	const float texelY = 1.0f / static_cast<float>(height);

	device->SetRenderTarget(0, g_bloomFront.Surface());
	FullScreenQuad::SetConstant(device, 0, 1.0f / static_cast<float>(desc.Width),
		1.0f / static_cast<float>(desc.Height), 0.0f, 0.0f);
	FullScreenQuad::SetConstant(device, 1, Positive(g_modVals.bloomThreshold), 0.25f, 0.0f, 0.0f);
	FullScreenQuad::Draw(device, g_bloomBright.Get(), source.Texture(), width, height, true);

	device->SetRenderTarget(0, g_bloomBack.Surface());
	FullScreenQuad::SetConstant(device, 0, texelX, 0.0f, 0.0f, 0.0f);
	FullScreenQuad::Draw(device, g_bloomBlur.Get(), g_bloomFront.Texture(), width, height, true);

	device->SetRenderTarget(0, g_bloomFront.Surface());
	FullScreenQuad::SetConstant(device, 0, 0.0f, texelY, 0.0f, 0.0f);
	FullScreenQuad::Draw(device, g_bloomBlur.Get(), g_bloomBack.Texture(), width, height, true);

	device->SetRenderTarget(0, destination);
	FullScreenQuad::SetConstant(device, 0,
		static_cast<float>(g_modVals.bloomIntensity) / 50.0f, 0.0f, 0.0f, 0.0f);
	FullScreenQuad::Draw(device, g_bloomMix.Get(), source.Texture(), desc.Width, desc.Height, true,
		g_bloomFront.Texture());
}

void RunStage(IDirect3DDevice9* device, int stage, IDirect3DPixelShader9* shader,
	ScratchTarget& source, IDirect3DSurface9* destination, const D3DSURFACE_DESC& desc)
{
	device->SetDepthStencilSurface(nullptr);

	if (stage == Stage_Bloom)
	{
		RunBloom(device, source, destination, desc);
		return;
	}

	device->SetRenderTarget(0, destination);
	SetStageConstants(device, stage, desc.Width, desc.Height);

	FullScreenQuad::Draw(device, shader, source.Texture(), desc.Width, desc.Height,
		StageWantsLinear(stage));
}

bool EnsureTargets(IDirect3DDevice9* device, const D3DSURFACE_DESC& desc, int stageCount,
	bool bloom)
{
	if (!g_front.Ensure(device, desc.Width, desc.Height, desc.Format))
		return false;

	if (stageCount > 1 && !g_back.Ensure(device, desc.Width, desc.Height, desc.Format))
		return false;

	return !bloom || EnsureBloomTargets(device, desc);
}

const char* StageName(int stage)
{
	switch (stage)
	{
	case Stage_Fxaa:
		return "AA";
	case Stage_Bloom:
		return "bloom";
	case Stage_Look:
		return "look";
	case Stage_Sharpen:
		return "sharpen";
	default:
		return "pack";
	}
}

void DescribeRun(const int* stages, int drawn, int asked, unsigned width, unsigned height)
{
	char list[128] = {};
	int written = 0;

	for (int i = 0; i < drawn && written < static_cast<int>(sizeof(list)) - 1; ++i)
	{
		written += snprintf(list + written, sizeof(list) - written, "%s%s", written > 0 ? " + " : "",
			StageName(stages[i]));
	}

	if (drawn == asked)
	{
		Report("%s over %ux%u", list, width, height);
		return;
	}

	Report("%s over %ux%u, %d of %d stages ran - the rest have no shader on this device", list,
		width, height, drawn, asked);
}

}

bool PostChain::IsLookNeutral()
{
	return g_modVals.lookBrightness == 0 && g_modVals.lookContrast == 0 &&
		g_modVals.lookSaturation == 0 && g_modVals.lookVibrance == 0 &&
		g_modVals.lookTemperature == 0 && g_modVals.lookVignette == 0 &&
		g_modVals.lookScanlines == 0 && g_modVals.lookGamma == 100 && !g_modVals.lookDither;
}

bool PostChain::IsAnyStageOn()
{
	int stages[Stage_COUNT] = {};

	return CollectStages(stages) > 0;
}

void PostChain::ResetLook()
{
	g_modVals.lookBrightness = 0;
	g_modVals.lookContrast = 0;
	g_modVals.lookSaturation = 0;
	g_modVals.lookVibrance = 0;
	g_modVals.lookTemperature = 0;
	g_modVals.lookVignette = 0;
	g_modVals.lookScanlines = 0;
	g_modVals.lookGamma = 100;
	g_modVals.lookDither = false;

	Settings::SaveInt("Graphics", "LookBrightness", 0);
	Settings::SaveInt("Graphics", "LookContrast", 0);
	Settings::SaveInt("Graphics", "LookSaturation", 0);
	Settings::SaveInt("Graphics", "LookVibrance", 0);
	Settings::SaveInt("Graphics", "LookTemperature", 0);
	Settings::SaveInt("Graphics", "LookVignette", 0);
	Settings::SaveInt("Graphics", "LookScanlines", 0);
	Settings::SaveInt("Graphics", "LookGamma", 100);
	Settings::SaveInt("Graphics", "LookDither", 0);
}

void PostChain::TurnOff()
{
	g_modVals.antiAliasing = AntiAlias::Level_Off;
	g_modVals.sharpenMode = SharpenMode::Kind_Off;
	g_modVals.sharpenStrength = 0;
	g_modVals.bloomEnabled = false;
	g_modVals.lookEnabled = false;

	Settings::SaveInt("Graphics", "AntiAliasing", 0);
	Settings::SaveInt("Graphics", "SharpenMode", 0);
	Settings::SaveInt("Graphics", "Sharpen", 0);
	Settings::SaveInt("Graphics", "Bloom", 0);
	Settings::SaveInt("Graphics", "Look", 0);

	ShaderPack::Select(-1);
	ResetLook();
}

void PostChain::Apply(IDirect3DDevice9* device)
{
	++g_frames;

	int wanted[Stage_COUNT] = {};
	const int count = CollectStages(wanted);

	if (count == 0 || device == nullptr)
	{
		snprintf(g_status, sizeof(g_status), "off");
		return;
	}

	if (g_failed)
		return;

	int stages[Stage_COUNT] = {};
	IDirect3DPixelShader9* shaders[Stage_COUNT] = {};
	int resolved = 0;
	bool bloom = false;

	for (int i = 0; i < count; ++i)
	{
		IDirect3DPixelShader9* const shader = ShaderFor(device, wanted[i]);

		if (shader == nullptr)
			continue;

		bloom = bloom || wanted[i] == Stage_Bloom;
		stages[resolved] = wanted[i];
		shaders[resolved] = shader;
		++resolved;
	}

	if (resolved == 0)
	{
		Report("%d stage%s asked for and none of them has a shader on this device", count,
			count == 1 ? "" : "s");
		return;
	}

	IDirect3DSurface9* backBuffer = nullptr;
	if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) ||
		backBuffer == nullptr)
	{
		return;
	}

	D3DSURFACE_DESC desc = {};
	if (FAILED(backBuffer->GetDesc(&desc)) || !EnsureTargets(device, desc, resolved, bloom))
	{
		backBuffer->Release();
		Report("no room for a %ux%u working copy", desc.Width, desc.Height);
		return;
	}

	if (FAILED(device->StretchRect(backBuffer, nullptr, g_front.Surface(), nullptr, D3DTEXF_NONE)))
	{
		backBuffer->Release();
		Report("the back buffer cannot be copied on this device");
		g_failed = true;
		LOG("[PostChain] %s", g_status);
		return;
	}

	if (!g_state.Capture(device))
	{
		backBuffer->Release();
		return;
	}

	const bool opened = SUCCEEDED(device->BeginScene());

	ScratchTarget* source = &g_front;
	ScratchTarget* spare = &g_back;

	for (int i = 0; i < resolved; ++i)
	{
		const bool last = i == resolved - 1;

		RunStage(device, stages[i], shaders[i], *source, last ? backBuffer : spare->Surface(),
			desc);

		ScratchTarget* const used = source;
		source = spare;
		spare = used;
	}

	if (opened)
		device->EndScene();

	g_state.Restore();
	backBuffer->Release();

	DescribeRun(stages, resolved, count, desc.Width, desc.Height);
}

void PostChain::OnDeviceLost()
{
	g_state.Release();
	g_front.Release();
	g_back.Release();
	g_bloomFront.Release();
	g_bloomBack.Release();
	ShaderPack::OnDeviceLost();
}

void PostChain::Shutdown()
{
	OnDeviceLost();

	g_fxaa.Release();
	g_sharpen.Release();
	g_look.Release();
	g_bloomBright.Release();
	g_bloomBlur.Release();
	g_bloomMix.Release();
}

const char* PostChain::GetStatusText()
{
	return g_status;
}
