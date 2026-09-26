#include "D3D9/Post/PostStages.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "D3D9/Post/PostOptions.h"
#include "D3D9/Post/UpscaleFilter.h"

namespace {

void Store(int& field, const char* key, int value)
{
	field = value;
	Settings::SaveInt("Graphics", key, value);
}

void Store(bool& field, const char* key, bool value)
{
	field = value;
	Settings::SaveInt("Graphics", key, value ? 1 : 0);
}

}

int PostStages::GetUpscaleFilter()
{
	return UpscaleFilter::Clamp(g_modVals.upscaleFilter);
}

void PostStages::SetUpscaleFilter(int kind)
{
	Store(g_modVals.upscaleFilter, "UpscaleFilter", UpscaleFilter::Clamp(kind));
}

int PostStages::GetAntiAliasing()
{
	return AntiAlias::Clamp(g_modVals.antiAliasing);
}

void PostStages::SetAntiAliasing(int level)
{
	Store(g_modVals.antiAliasing, "AntiAliasing", AntiAlias::Clamp(level));
}

int PostStages::GetSharpening()
{
	return SharpenMode::Clamp(g_modVals.sharpenMode);
}

void PostStages::SetSharpening(int kind)
{
	Store(g_modVals.sharpenMode, "SharpenMode", SharpenMode::Clamp(kind));
}

bool PostStages::IsBloomOn()
{
	return g_modVals.bloomEnabled;
}

void PostStages::SetBloom(bool on)
{
	Store(g_modVals.bloomEnabled, "Bloom", on);
}

bool PostStages::IsLookOn()
{
	return g_modVals.lookEnabled;
}

void PostStages::SetLook(bool on)
{
	Store(g_modVals.lookEnabled, "Look", on);
}
