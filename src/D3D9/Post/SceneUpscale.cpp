#include "D3D9/Post/SceneUpscale.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "D3D9/Post/DeviceState.h"
#include "D3D9/Post/FullScreenPass.h"
#include "D3D9/Post/ScratchTarget.h"
#include "D3D9/SceneScale.h"
#include "D3D9/Post/UpscaleFilter.h"

#include <cstdarg>
#include <cstdio>

namespace {

constexpr int kMaxPassesPerFrame = 2;

PixelShaderHandle g_shader;
ScratchTarget g_target;
DeviceState g_state;

int g_kind = UpscaleFilter::Kind_Off;

IDirect3DBaseTexture9* g_source = nullptr;
int g_passesThisFrame = 0;
bool g_reportedThisFrame = false;
bool g_running = false;
bool g_failed = false;
bool g_announced = false;

char g_status[224] = "off";

void Report(const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(g_status, sizeof(g_status), format, arguments);
	va_end(arguments);

	g_reportedThisFrame = true;
}

int WantedKind()
{
	return UpscaleFilter::Clamp(g_modVals.upscaleFilter);
}

bool EnsurePass(IDirect3DDevice9* device, int kind)
{
	const void* bytecode = UpscaleFilter::GetBytecode(kind);

	if (bytecode == nullptr)
		return false;

	if (g_shader.Ensure(device, bytecode))
	{
		g_kind = kind;
		return true;
	}

	Report("the device refused the %s shader - it needs pixel shader 3.0",
		UpscaleFilter::GetName(kind));
	return false;
}

IDirect3DTexture9* AsRenderTargetTexture(IDirect3DBaseTexture9* texture, D3DSURFACE_DESC& outDesc)
{
	if (texture == nullptr || texture->GetType() != D3DRTYPE_TEXTURE)
		return nullptr;

	IDirect3DTexture9* flat = static_cast<IDirect3DTexture9*>(texture);

	if (FAILED(flat->GetLevelDesc(0, &outDesc)) || (outDesc.Usage & D3DUSAGE_RENDERTARGET) == 0)
		return nullptr;

	return flat;
}

bool IsSceneTarget(const D3DSURFACE_DESC& desc)
{
	int width = 0;
	int height = 0;

	if (!SceneScale::GetSize(width, height))
	{
		Report("the engine's render size globals did not read - refusing");
		return false;
	}

	return desc.Width == static_cast<unsigned>(width) &&
		desc.Height == static_cast<unsigned>(height);
}

bool TargetIsBackBuffer(IDirect3DDevice9* device, D3DSURFACE_DESC& outDesc)
{
	IDirect3DSurface9* current = nullptr;
	if (FAILED(device->GetRenderTarget(0, &current)) || current == nullptr)
		return false;

	IDirect3DSurface9* backBuffer = nullptr;
	if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) ||
		backBuffer == nullptr)
	{
		current->Release();
		return false;
	}

	const bool same = current == backBuffer && SUCCEEDED(backBuffer->GetDesc(&outDesc));

	backBuffer->Release();
	current->Release();
	return same;
}

bool Run(IDirect3DDevice9* device, IDirect3DTexture9* source, const D3DSURFACE_DESC& sourceDesc,
	const D3DSURFACE_DESC& backBufferDesc)
{
	if (!g_target.Ensure(device, backBufferDesc.Width, backBufferDesc.Height, sourceDesc.Format))
	{
		Report("no room for a %ux%u copy", backBufferDesc.Width, backBufferDesc.Height);
		return false;
	}

	if (!g_state.Capture(device))
		return false;

	const bool opened = SUCCEEDED(device->BeginScene());

	device->SetRenderTarget(0, g_target.Surface());
	device->SetDepthStencilSurface(nullptr);

	FullScreenQuad::SetConstant(device, 0, static_cast<float>(sourceDesc.Width),
		static_cast<float>(sourceDesc.Height), 1.0f / static_cast<float>(sourceDesc.Width),
		1.0f / static_cast<float>(sourceDesc.Height));

	FullScreenQuad::Draw(device, g_shader.Get(), source, g_target.Width(), g_target.Height(),
		UpscaleFilter::WantsLinear(g_kind));

	if (opened)
		device->EndScene();

	g_state.Restore();
	return true;
}

}

IDirect3DBaseTexture9* SceneUpscale::OnSetTexture(IDirect3DDevice9* device, DWORD stage,
	IDirect3DBaseTexture9* texture)
{
	const int kind = WantedKind();

	if (kind == UpscaleFilter::Kind_Off || g_running || g_failed || stage != 0 || device == nullptr)
		return texture;

	if (texture != nullptr && texture == g_source)
		return g_target.Texture();

	if (g_passesThisFrame >= kMaxPassesPerFrame)
		return texture;

	D3DSURFACE_DESC sourceDesc = {};
	IDirect3DTexture9* source = AsRenderTargetTexture(texture, sourceDesc);
	if (source == nullptr)
		return texture;

	if (!IsSceneTarget(sourceDesc))
		return texture;

	D3DSURFACE_DESC backBufferDesc = {};
	if (!TargetIsBackBuffer(device, backBufferDesc))
		return texture;

	if (backBufferDesc.Width <= sourceDesc.Width || backBufferDesc.Height <= sourceDesc.Height)
	{
		Report("the scene is %ux%u into a %ux%u back buffer, so there is nothing to magnify - "
			"raise the Improvements level or lower the scene resolution", sourceDesc.Width,
			sourceDesc.Height, backBufferDesc.Width, backBufferDesc.Height);
		return texture;
	}

	if (!EnsurePass(device, kind))
	{
		g_failed = true;
		LOG("[SceneUpscale] %s", g_status);
		return texture;
	}

	g_running = true;
	const bool ran = Run(device, source, sourceDesc, backBufferDesc);
	g_running = false;

	if (!ran)
		return texture;

	g_source = texture;
	++g_passesThisFrame;

	Report("%s, %ux%u to %ux%u", UpscaleFilter::GetName(g_kind), sourceDesc.Width,
		sourceDesc.Height, g_target.Width(), g_target.Height());

	if (!g_announced)
	{
		g_announced = true;
		LOG("[SceneUpscale] %s", g_status);
	}

	return g_target.Texture();
}

void SceneUpscale::OnPresent()
{
	if (WantedKind() == UpscaleFilter::Kind_Off)
		snprintf(g_status, sizeof(g_status), "off");
	else if (!g_reportedThisFrame && !g_failed)
		snprintf(g_status, sizeof(g_status), "no composite draw seen this frame");

	g_source = nullptr;
	g_passesThisFrame = 0;
	g_reportedThisFrame = false;
}

void SceneUpscale::OnDeviceLost()
{
	g_state.Release();
	g_target.Release();
	g_source = nullptr;
	g_passesThisFrame = 0;
}

void SceneUpscale::Shutdown()
{
	OnDeviceLost();

	g_shader.Release();
	g_kind = UpscaleFilter::Kind_Off;
	g_announced = false;
}

const char* SceneUpscale::GetStatusText()
{
	return g_status;
}
