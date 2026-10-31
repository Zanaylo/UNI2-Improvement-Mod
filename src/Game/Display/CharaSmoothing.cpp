#include "Game/Display/CharaSmoothing.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "D3D9/Device/ScaledTargets.h"
#include "Game/Display/CharaShaderText.h"

#include <cstdio>

namespace {

constexpr DWORD kSpriteStage = 0;
constexpr UINT kOneRegister = 1;

volatile long g_live = 0;
char g_status[128] = "off";

void Upload(IDirect3DDevice9* device, const float (&texel)[4])
{
	device->SetPixelShaderConstantF(CharaShaderText::kTexelRegister, texel, kOneRegister);
}

bool SpriteTexel(IDirect3DDevice9* device, float (&texel)[4])
{
	IDirect3DBaseTexture9* texture = nullptr;

	if (FAILED(device->GetTexture(kSpriteStage, &texture)) || texture == nullptr)
		return false;

	D3DSURFACE_DESC desc = {};
	const bool known = texture->GetType() == D3DRTYPE_TEXTURE &&
		SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0, &desc)) && desc.Width > 0 &&
		desc.Height > 0;

	texture->Release();

	if (!known)
		return false;

	texel[0] = 1.0f / static_cast<float>(desc.Width);
	texel[1] = 1.0f / static_cast<float>(desc.Height);
	texel[2] = static_cast<float>(desc.Width);
	texel[3] = static_cast<float>(desc.Height);
	return true;
}

}

bool CharaSmoothing::Rewrite(const char* source, UINT bytes, std::string& out)
{
	if (!ScaledTargets::ScalesSceneLayer())
		return false;

	int replaced = 0;

	if (!CharaShaderText::Rewrite(std::string(source, bytes), out, replaced))
		return false;

	LOG("CharaSmoothing: %d character colour lookup(s) sample smoothly when magnified", replaced);
	return true;
}

void CharaSmoothing::NoteCompiled(bool compiled)
{
	InterlockedExchange(&g_live, compiled ? 1 : 0);
	sprintf_s(g_status, compiled ? "on, characters are smoothed where they are drawn larger"
		: "the smoothed character shader did not compile, the game's shader is used");
	LOG("CharaSmoothing: %s", g_status);
}

void CharaSmoothing::BeforeDraw(IDirect3DDevice9* device)
{
	if (InterlockedCompareExchange(&g_live, 0, 0) == 0)
		return;

	float texel[4] = {};

	if (g_modVals.smoothCharacters && ScaledTargets::BoundIsScaled())
		SpriteTexel(device, texel);

	Upload(device, texel);
}

const char* CharaSmoothing::StatusText()
{
	return g_status;
}
