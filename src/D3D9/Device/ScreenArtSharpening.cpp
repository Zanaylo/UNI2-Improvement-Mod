#include "D3D9/Device/ScreenArtSharpening.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "D3D9/Device/MagnifyRule.h"
#include "D3D9/Device/ScaledTargets.h"
#include "D3D9/Device/ScreenArtRule.h"
#include "D3D9/Post/Shaders/ArtUpscaleShader.h"
#include "D3D9/Post/Shaders/TextSharpenShader.h"
#include "Game/Display/FontPages.h"

namespace {

constexpr DWORD kStage = 0;
constexpr DWORD kNextStage = 1;
constexpr UINT kFirstRegister = 0;
constexpr UINT kRegisters = 2;

struct LookShader
{
	const void* bytecode;
	const char* name;
	IDirect3DPixelShader9* shader;
	bool failed;
	bool used;
};

LookShader g_shaders[] = {
	{ nullptr, "", nullptr, true, false },
	{ kTextSharpenShader, "the game's text is redrawn with sharp edges", nullptr, false, false },
	{ kArtUpscaleShader, "menu and HUD art is upscaled edge-aware", nullptr, false, false },
};

DWORD StageState(IDirect3DDevice9* device, DWORD stage, D3DTEXTURESTAGESTATETYPE type)
{
	DWORD value = 0;
	device->GetTextureStageState(stage, type, &value);
	return value;
}

bool HasPixelShader(IDirect3DDevice9* device)
{
	IDirect3DPixelShader9* shader = nullptr;

	if (FAILED(device->GetPixelShader(&shader)) || shader == nullptr)
		return false;

	shader->Release();
	return true;
}

bool ArtSize(IDirect3DBaseTexture9* art, D3DSURFACE_DESC& desc)
{
	return art->GetType() == D3DRTYPE_TEXTURE &&
		SUCCEEDED(static_cast<IDirect3DTexture9*>(art)->GetLevelDesc(0, &desc)) && desc.Width > 0 && desc.Height > 0;
}

ScreenArtRule::Stage ReadStage(IDirect3DDevice9* device, IDirect3DBaseTexture9* art, const D3DSURFACE_DESC& desc)
{
	ScreenArtRule::Stage stage = {};
	stage.fontPage = FontPages::IsPage(art);
	stage.renderTarget = (desc.Usage & D3DUSAGE_RENDERTARGET) != 0 || ScaledTargets::IsScaledTexture(art);
	stage.holdsIndices = MagnifyRule::HoldsIndices(desc.Format);
	stage.sharpText = g_modVals.sharpText != 0;
	stage.scale = ScaledTargets::ScaleY();
	stage.pixelShader = HasPixelShader(device);
	stage.colourOp = StageState(device, kStage, D3DTSS_COLOROP);
	stage.colourArg1 = StageState(device, kStage, D3DTSS_COLORARG1);
	stage.colourArg2 = StageState(device, kStage, D3DTSS_COLORARG2);
	stage.alphaOp = StageState(device, kStage, D3DTSS_ALPHAOP);
	stage.alphaArg1 = StageState(device, kStage, D3DTSS_ALPHAARG1);
	stage.alphaArg2 = StageState(device, kStage, D3DTSS_ALPHAARG2);
	stage.nextColourOp = StageState(device, kNextStage, D3DTSS_COLOROP);
	return stage;
}

IDirect3DPixelShader9* ShaderFor(IDirect3DDevice9* device, ScreenArtRule::Look look)
{
	LookShader& entry = g_shaders[look];

	if (entry.shader != nullptr || entry.failed)
		return entry.shader;

	entry.failed = FAILED(device->CreatePixelShader(static_cast<const DWORD*>(entry.bytecode), &entry.shader));

	if (entry.failed)
		LOG("ScreenArt: a shader could not be created, that art is drawn as the game draws it");

	return entry.shader;
}

void NoteFirstUse(ScreenArtRule::Look look)
{
	LookShader& entry = g_shaders[look];

	if (entry.used)
		return;

	entry.used = true;
	LOG("ScreenArt: %s at %.2fx", entry.name, ScaledTargets::ScaleY());
}

}

ScreenArtSharpening::ScreenArtSharpening(IDirect3DDevice9* device)
	: m_device(device)
{
	if (!ScaledTargets::ScalesSceneLayer() || !ScaledTargets::BoundIsScaled())
		return;

	IDirect3DBaseTexture9* art = nullptr;

	if (FAILED(device->GetTexture(kStage, &art)) || art == nullptr)
		return;

	m_applied = Apply(art);
	art->Release();
}

bool ScreenArtSharpening::Apply(IDirect3DBaseTexture9* art)
{
	D3DSURFACE_DESC desc = {};

	if (!ArtSize(art, desc))
		return false;

	const ScreenArtRule::Look look = ScreenArtRule::Choose(ReadStage(m_device, art, desc));

	if (look == ScreenArtRule::Look_Game)
		return false;

	IDirect3DPixelShader9* const shader = ShaderFor(m_device, look);

	if (shader == nullptr)
		return false;

	const float constants[kRegisters][4] = {
		{ 1.0f / desc.Width, 1.0f / desc.Height, static_cast<float>(desc.Width), static_cast<float>(desc.Height) },
		{ ScreenArtRule::Steepness(ScaledTargets::ScaleY()), 0.0f, 0.0f, 0.0f },
	};

	m_device->GetPixelShaderConstantF(kFirstRegister, &m_constants[0][0], kRegisters);
	m_device->GetSamplerState(kStage, D3DSAMP_MAGFILTER, &m_magFilter);
	m_device->GetSamplerState(kStage, D3DSAMP_MINFILTER, &m_minFilter);

	m_device->SetPixelShader(shader);
	m_device->SetPixelShaderConstantF(kFirstRegister, &constants[0][0], kRegisters);
	m_device->SetSamplerState(kStage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	m_device->SetSamplerState(kStage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

	NoteFirstUse(look);
	return true;
}

ScreenArtSharpening::~ScreenArtSharpening()
{
	Withdraw();
}

void ScreenArtSharpening::Withdraw()
{
	if (!m_applied)
		return;

	m_applied = false;
	m_device->SetPixelShader(nullptr);
	m_device->SetPixelShaderConstantF(kFirstRegister, &m_constants[0][0], kRegisters);
	m_device->SetSamplerState(kStage, D3DSAMP_MAGFILTER, m_magFilter);
	m_device->SetSamplerState(kStage, D3DSAMP_MINFILTER, m_minFilter);
}
