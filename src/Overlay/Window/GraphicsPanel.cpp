#include "Overlay/Window/GraphicsPanel.h"

#include "Core/DpiScaling.h"
#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "D3D9/DeviceHooks.h"
#include "D3D9/Post/PostChain.h"
#include "D3D9/Post/PostOptions.h"
#include "D3D9/Post/SceneUpscale.h"
#include "D3D9/Post/ShaderPack.h"
#include "D3D9/Post/UpscaleFilter.h"
#include "Game/EngineQuality.h"
#include "Game/Improvements.h"
#include "Game/PotatoMode.h"
#include "Overlay/ComboNav.h"
#include "Overlay/OverlayFont.h"
#include "Overlay/UiText.h"
#include "Training/StageColor.h"

#include "Overlay/UiScale.h"

#include <imgui.h>

using UiText::Good;
using UiText::Help;
using UiText::Muted;
using UiText::Warn;

namespace {

constexpr float kSliderWidth = 240.0f;

bool RadioRow(const char* id, int count, const char* (*name)(int), int current, int& outChosen)
{
	ImGui::PushID(id);

	bool changed = false;

	for (int candidate = 0; candidate < count; ++candidate)
	{
		if (candidate > 0)
			ImGui::SameLine();

		if (!ImGui::RadioButton(name(candidate), current == candidate) || candidate == current)
			continue;

		outChosen = candidate;
		changed = true;
	}

	ImGui::PopID();
	return changed;
}

bool SavedSlider(const char* label, int* value, int lowest, int highest, const char* key,
	const char* format)
{
	Ui::SetItemWidth(kSliderWidth);

	const bool changed = ImGui::SliderInt(label, value, lowest, highest, format);

	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("Graphics", key, *value);

	return changed;
}

void DrawUpscaleFilter()
{
	ImGui::SeparatorText("Upscale filter");

	const int current = UpscaleFilter::Clamp(g_modVals.upscaleFilter);
	int chosen = current;

	if (RadioRow("filter", UpscaleFilter::Kind_COUNT, &UpscaleFilter::GetName, current, chosen))
	{
		g_modVals.upscaleFilter = chosen;
		Settings::SaveInt("Graphics", "UpscaleFilter", chosen);
	}

	Help("The game rasterises the whole scene at 1280x720 and stretches it to your window with a "
		"plain bilinear filter. That stretch is the only magnification in the frame, so it is the "
		"one place a better kernel can reach. Nothing is patched: the engine draws as it always "
		"does and is handed a texture already the size it is about to draw at.\n\n"
		"It needs a back buffer larger than 1280x720, so it does nothing unless the present size "
		"above is raised.");

	Muted("%s", UpscaleFilter::Describe(current));
	Muted("%s", SceneUpscale::GetStatusText());
}

void DrawAntiAliasing()
{
	ImGui::SeparatorText("Anti-aliasing");

	const int current = AntiAlias::Clamp(g_modVals.antiAliasing);
	int chosen = current;

	if (RadioRow("aa", AntiAlias::Level_COUNT, &AntiAlias::GetName, current, chosen))
	{
		g_modVals.antiAliasing = chosen;
		Settings::SaveInt("Graphics", "AntiAliasing", chosen);
	}

	Help("FXAA over the finished frame.\n\n"
		"Multisampling is not offered because it cannot reach this game: a Direct3D 9 texture "
		"cannot be multisampled and the whole scene is drawn into textures, so the samples would "
		"be spent on the single quad the finished picture is drawn with. Supersampling - the "
		"present size above - and this filter are the two things that can.");

	Muted("%s", AntiAlias::Describe(current));
}

void DrawBloom()
{
	ImGui::SeparatorText("Bloom");

	if (ImGui::Checkbox("Bloom", &g_modVals.bloomEnabled))
		Settings::SaveInt("Graphics", "Bloom", g_modVals.bloomEnabled ? 1 : 0);

	Help("The bright parts of the picture are cut out, blurred at a quarter of the size and "
		"screened back on. It is the one thing here that reads as lighting rather than as "
		"filtering - the neon in these stages, the moon, and the glow on EXS and super effects are "
		"what it is for.\n\n"
		"Threshold is how bright a pixel has to be before it glows. Lower it and the whole frame "
		"starts to haze; raise it and only the real highlights bloom.");

	if (!g_modVals.bloomEnabled)
		return;

	ImGui::Indent();
	SavedSlider("Intensity", &g_modVals.bloomIntensity, 0, 100, "BloomIntensity", "%d%%");
	SavedSlider("Threshold", &g_modVals.bloomThreshold, 0, 100, "BloomThreshold", "%d%%");
	ImGui::Unindent();
}

void DrawSharpening()
{
	ImGui::SeparatorText("Sharpening");

	const int mode = SharpenMode::Clamp(g_modVals.sharpenMode);
	int chosen = mode;

	if (RadioRow("sharpen", SharpenMode::Kind_COUNT, &SharpenMode::GetName, mode, chosen))
	{
		g_modVals.sharpenMode = chosen;
		Settings::SaveInt("Graphics", "SharpenMode", chosen);
	}

	Help("The softness in this game is in the stretch to your window rather than in the art, so "
		"putting the edge contrast back is most of what a higher resolution would have looked "
		"like. 40-60% is the useful range.");

	Muted("%s", SharpenMode::Describe(mode));

	if (mode == SharpenMode::Kind_Off)
		return;

	ImGui::Indent();
	SavedSlider("Strength", &g_modVals.sharpenStrength, 0, 100, "Sharpen",
		g_modVals.sharpenStrength > 0 ? "%d%%" : "off");
	ImGui::Unindent();
}

void DrawLook()
{
	ImGui::SeparatorText("Colour and display");

	if (ImGui::Checkbox("Colour and display", &g_modVals.lookEnabled))
		Settings::SaveInt("Graphics", "Look", g_modVals.lookEnabled ? 1 : 0);

	Help("One pass over the finished frame. Off, none of the values below is read and no pass is "
		"drawn.");

	if (!g_modVals.lookEnabled)
		return;

	ImGui::SameLine();

	if (ImGui::Button("Reset"))
		PostChain::ResetLook();

	ImGui::Indent();

	SavedSlider("Brightness", &g_modVals.lookBrightness, -100, 100, "LookBrightness", "%d");
	SavedSlider("Contrast", &g_modVals.lookContrast, -100, 100, "LookContrast", "%d");
	SavedSlider("Gamma", &g_modVals.lookGamma, 25, 400, "LookGamma", "%d%%");
	SavedSlider("Saturation", &g_modVals.lookSaturation, -100, 100, "LookSaturation", "%d");

	SavedSlider("Vibrance", &g_modVals.lookVibrance, -100, 100, "LookVibrance", "%d");
	Help("Saturation that leaves the colours already vivid alone and lifts the ones that are not.");

	SavedSlider("Warmth", &g_modVals.lookTemperature, -100, 100, "LookTemperature", "%d");
	SavedSlider("Vignette", &g_modVals.lookVignette, 0, 100, "LookVignette", "%d%%");
	SavedSlider("Scanlines", &g_modVals.lookScanlines, 0, 100, "LookScanlines", "%d%%");

	if (ImGui::Checkbox("Dither", &g_modVals.lookDither))
		Settings::SaveInt("Graphics", "LookDither", g_modVals.lookDither ? 1 : 0);

	Help("A pixel of noise under the banding a gradient picks up on an 8 bit back buffer.");

	ImGui::Unindent();
}

void DrawShaderPacks()
{
	ImGui::SeparatorText("Shader pack");

	const int selected = ShaderPack::GetSelected();

	Ui::SetItemWidth(kSliderWidth);

	if (ImGui::BeginCombo("Pack", selected < 0 ? "Off" : ShaderPack::GetName(selected)))
	{
		if (ImGui::Selectable("Off", selected < 0))
			ShaderPack::Select(-1);

		ComboNav::KeepSelectedInView(selected < 0);

		for (int i = 0; i < ShaderPack::Count(); ++i)
		{
			const bool chosen = i == selected;

			ImGui::PushID(i);

			if (ImGui::Selectable(ShaderPack::GetName(i), chosen))
				ShaderPack::Select(i);

			ComboNav::KeepSelectedInView(chosen);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();
	const int target = selected + steps;

	if (steps != 0 && target >= -1 && target < ShaderPack::Count())
		ShaderPack::Select(target);

	ImGui::SameLine();

	if (ImGui::Button("Rescan"))
		ShaderPack::Refresh();

	Help("Drop a shader in the Shaders folder beside the ini and pick it here. It is compiled when "
		"you select it and runs last in the chain, over the finished frame.\n\n"
		"Taken: .hlsl and .ps as they are, a ReShade .fx, and libretro or Shadertoy GLSL as .slang, "
		".glsl, .frag or .fsh. Everything but HLSL is translated on the way in and the translation "
		"is written to the Translated folder, so a shader that comes out wrong can be read and "
		"fixed there.\n\n"
		"It is one pass and nothing else: a shader that needs a second pass, a lookup texture, the "
		"depth buffer or the previous frame will translate and then be wrong. The folder's README "
		"has the bindings and what each format is given.\n\n"
		"Compiling needs d3dcompiler_47.dll, which ships with Windows and with Proton.");

	Muted("%s", ShaderPack::GetStatusText());
}

}

bool GraphicsPanel::DrawEverythingOff()
{
	if (!ImGui::Button("Everything off"))
	{
		Help("Puts every graphics setting in the mod back to the game's own, on this tab and the "
			"other two: present size, all five shader stages, the back buffer's multisampling, "
			"Character Visual Improvements, the empty stage and POTATO MODE. After this the mod "
			"draws nothing into the frame and reads nothing out of it.");
		return false;
	}

	PostChain::TurnOff();

	g_modVals.upscaleFilter = UpscaleFilter::Kind_Off;
	g_modVals.disableBackBufferAa = false;
	g_modVals.disableCharacterFilter = false;
	g_modVals.simpleStage = false;

	Settings::SaveInt("Graphics", "UpscaleFilter", 0);
	Settings::SaveInt("Graphics", "DisableBackBufferAA", 0);
	Settings::SaveInt("Graphics", "DisableCharacterFilter", 0);
	Settings::SaveInt("Graphics", "SimpleStage", 0);
	Settings::SaveInt("Video", "FlatStage", 0);

	StageColor::SetEnabled(false);
	EngineQuality::Apply();
	PotatoMode::Apply(PotatoMode::Level_Off);
	Improvements::Apply(Improvements::Level_Off);

	return true;
}

void GraphicsPanel::DrawShadersTab()
{
	ImGui::Spacing();

	DrawEverythingOff();

	ImGui::SameLine();
	Muted("Nothing here reaches the simulation, the inputs, or anything an opponent sees.");

	DrawUpscaleFilter();
	DrawAntiAliasing();
	DrawBloom();
	DrawSharpening();
	DrawLook();
	DrawShaderPacks();

	ImGui::Spacing();
	ImGui::SeparatorText("In force now");
	Muted("%s", PostChain::GetStatusText());
}

void GraphicsPanel::DrawOverlayAppearance()
{
	ImGui::SeparatorText("Overlay appearance");

	Ui::SetItemWidth(kSliderWidth);
	ImGui::SliderFloat("Overlay scale", &g_modVals.uiScale, 0.5f, 4.0f, "%.2fx");

	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveFloat("Overlay", "UiScale", g_modVals.uiScale);

	Ui::SetItemWidth(kSliderWidth);
	ImGui::SliderFloat("Font size", &g_modVals.fontSize, 10.0f, 32.0f, "%.0f px");

	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveFloat("Overlay", "FontSize", g_modVals.fontSize);

	Help("Both are live. Set [Overlay] FontPath in the ini to use a .ttf of your own.");

	Muted("%s", OverlayFont::GetStatusText());
	Muted("Japanese and the rest fall back to %s", OverlayFont::GetFallbackText());

	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	const float scale = DeviceHooks::GetOverlayScale();

	if (scale > 1.01f && scale < 1.99f)
	{
		Warn("The frame is drawn at %ux%u and fitted to a smaller window, and that ratio is not a "
			"whole number, so the overlay is resampled on the way down and its text softens. 4K "
			"into a 1080p window is exact and stays sharp; 1440p into 1080p is not. Raise the font "
			"size, or use 4K or Off.", present.BackBufferWidth, present.BackBufferHeight);
	}

	const int dpi = DpiScaling::GetWindowDpi();

	if (dpi > 96 && !DpiScaling::IsAware())
	{
		Warn("Windows is scaling this window by %d%%, so it renders small and is stretched - a "
			"second resampling over everything.", dpi * 100 / 96);
	}
	else if (DpiScaling::IsAware())
	{
		Good("Real pixels at %d DPI.", dpi);
	}

	if (ImGui::Checkbox("Handle display scaling ourselves", &g_modVals.dpiAware))
		Settings::SaveInt("Overlay", "DpiAware", g_modVals.dpiAware ? 1 : 0);

	Help("Takes effect on the next start, and it has to be set before the game makes its window. "
		"The game was not written for it, so it is off by default.");

	Muted("%s", DpiScaling::Describe());
}
