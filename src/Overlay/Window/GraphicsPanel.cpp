#include "Overlay/Window/GraphicsPanel.h"

#include "Core/AsyncFileDialog.h"
#include "Core/DpiScaling.h"
#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "D3D9/DeviceHooks.h"
#include "D3D9/DgVoodoo.h"
#include "D3D9/Dxvk.h"
#include "D3D9/GraphicsWrapper.h"
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

AsyncFileDialog g_dgVoodooDialog;
char g_dgVoodooNote[256] = {};

AsyncFileDialog g_dxvkDialog;
char g_dxvkNote[256] = {};

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

	Help("The game draws the scene at 1280x720 and stretches it to your window. This picks a "
		"better filter for that stretch.\n\n"
		"It does nothing until you raise the present size above 1280x720 on the Improvements tab.");

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
		"Multisampling cannot work here, because the game draws its scene into textures. For "
		"smoother edges, use this or raise the present size on the Improvements tab.");

	Muted("%s", AntiAlias::Describe(current));
}

void DrawBloom()
{
	ImGui::SeparatorText("Bloom");

	if (ImGui::Checkbox("Bloom", &g_modVals.bloomEnabled))
		Settings::SaveInt("Graphics", "Bloom", g_modVals.bloomEnabled ? 1 : 0);

	Help("Makes the bright parts of the picture glow: stage neon, the moon, EXS and super "
		"effects.\n\n"
		"Threshold is how bright a pixel must be to glow. Lower it and the whole picture gets "
		"hazy. Raise it and only the real highlights glow.");

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

	Help("Most of the game's softness comes from stretching it to your window. Sharpening brings "
		"the edges back. 40-60% works best.");

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

	Help("Adjusts the colours of the finished frame. When it is off, the sliders below do "
		"nothing.");

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
	Help("Boosts dull colours and leaves colours that are already vivid alone.");

	SavedSlider("Warmth", &g_modVals.lookTemperature, -100, 100, "LookTemperature", "%d");
	SavedSlider("Vignette", &g_modVals.lookVignette, 0, 100, "LookVignette", "%d%%");
	SavedSlider("Scanlines", &g_modVals.lookScanlines, 0, 100, "LookScanlines", "%d%%");

	if (ImGui::Checkbox("Dither", &g_modVals.lookDither))
		Settings::SaveInt("Graphics", "LookDither", g_modVals.lookDither ? 1 : 0);

	Help("Adds a tiny bit of noise to hide banding in smooth gradients.");

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

	Help("Put a shader in the Shaders folder next to the ini and pick it here. It runs last, over "
		"the finished frame.\n\n"
		"Supported: .hlsl, .ps, .fx, .slang, .glsl, .frag and .fsh. Anything that is not HLSL is "
		"translated and saved in the Translated folder, so you can fix it there.\n\n"
		"Only single pass shaders work. See the README in the folder for details.\n\n"
		"Needs d3dcompiler_47.dll, which comes with Windows and Proton.");

	Muted("%s", ShaderPack::GetStatusText());
}

void DrawDgVoodooChoices()
{
	for (int i = 0; i < DgVoodoo::Choice_COUNT; ++i)
	{
		const DgVoodoo::Choice choice = static_cast<DgVoodoo::Choice>(i);
		const int current = DgVoodoo::Chosen(choice);

		Ui::SetItemWidth(kSliderWidth);

		if (!ImGui::BeginCombo(DgVoodoo::ChoiceName(choice), DgVoodoo::ChoiceLabel(choice, current)))
			continue;

		for (int option = 0; option < DgVoodoo::ChoiceCount(choice); ++option)
		{
			if (ImGui::Selectable(DgVoodoo::ChoiceLabel(choice, option), option == current))
				DgVoodoo::Choose(choice, option);
		}

		ImGui::EndCombo();
	}

	if (DgVoodoo::Chosen(DgVoodoo::Choice_Filtering) != 0)
	{
		Warn("Forcing a filter breaks the character colours. Keep it on The game's own.");
	}

	for (int i = 0; i < DgVoodoo::Flag_COUNT; ++i)
	{
		const DgVoodoo::Flag flag = static_cast<DgVoodoo::Flag>(i);
		bool set = DgVoodoo::IsSet(flag);

		if (ImGui::Checkbox(DgVoodoo::FlagName(flag), &set))
			DgVoodoo::Set(flag, set);
	}

	int limit = DgVoodoo::FpsLimit();

	Ui::SetItemWidth(kSliderWidth);

	if (ImGui::SliderInt("Frame limit", &limit, 0, 360, limit == 0 ? "off" : "%d fps"))
		DgVoodoo::SetFpsLimit(limit);

	if (ImGui::IsItemDeactivatedAfterEdit())
		DgVoodoo::Save();
}

}

bool GraphicsPanel::DrawEverythingOff()
{
	if (!ImGui::Button("Everything off"))
	{
		Help("Turns off every graphics option in the mod: present size, shaders, back buffer "
			"multisampling, Character Visual Improvements, the empty stage and POTATO MODE. After "
			"this the mod draws nothing into the frame.");
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
	Muted("None of this changes gameplay, inputs or what your opponent sees.");

	DrawUpscaleFilter();
	DrawAntiAliasing();
	DrawBloom();
	DrawSharpening();
	DrawLook();
	DrawShaderPacks();

	ImGui::Spacing();
	ImGui::SeparatorText("Active now");
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

	Help("Both apply right away. To use your own .ttf font, set [Overlay] FontPath in the ini.");

	Muted("%s", OverlayFont::GetStatusText());
	Muted("Japanese and other scripts fall back to %s", OverlayFont::GetFallbackText());

	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	const float scale = DeviceHooks::GetOverlayScale();

	if (scale > 1.01f && scale < 1.99f)
	{
		Warn("The frame is drawn at %ux%u and shrunk to your window by an uneven ratio, so the "
			"overlay text looks soft. 4K into 1080p stays sharp, 1440p into 1080p does not. Raise "
			"the font size, or use 4K or Off.", present.BackBufferWidth, present.BackBufferHeight);
	}

	const int dpi = DpiScaling::GetWindowDpi();

	if (dpi > 96 && !DpiScaling::IsAware())
	{
		Warn("Windows is scaling this window by %d%%, so everything is drawn small and stretched, "
			"which blurs it.", dpi * 100 / 96);
	}
	else if (DpiScaling::IsAware())
	{
		Good("Real pixels at %d DPI.", dpi);
	}

	if (ImGui::Checkbox("Let the mod handle display scaling", &g_modVals.dpiAware))
		Settings::SaveInt("Overlay", "DpiAware", g_modVals.dpiAware ? 1 : 0);

	Help("Restart the game to apply. The game was not made for it, so it is off by default.");

	Muted("%s", DpiScaling::Describe());

	DrawMousePointer();
}

void GraphicsPanel::DrawMousePointer()
{
	ImGui::SeparatorText("Mouse pointer");

	const char* const modes[] = { "Automatic", "Always the mod's", "Always the Windows pointer" };

	Ui::SetItemWidth(kSliderWidth);

	if (ImGui::Combo("Overlay pointer", &g_modVals.overlayCursor, modes, IM_ARRAYSIZE(modes)))
		Settings::SaveInt("Overlay", "SoftwareCursor", g_modVals.overlayCursor);

	Help("Automatic draws the mod's pointer in exclusive fullscreen and when a Direct3D wrapper "
		"like dgVoodoo is running. In both cases the Windows pointer is hidden.");

	if (GraphicsWrapper::IsPresent())
		Warn("%s", GraphicsWrapper::StatusText());
	else
		Muted("%s", GraphicsWrapper::StatusText());
}

void GraphicsPanel::DrawDgVoodoo()
{
	ImGui::SeparatorText("dgVoodoo");

	std::string picked;

	if (g_dgVoodooDialog.TakeResult(picked) && !picked.empty())
		DgVoodoo::InstallFrom(picked, g_dgVoodooNote, sizeof(g_dgVoodooNote));

	bool enabled = DgVoodoo::IsEnabled();

	ImGui::BeginDisabled(!DgVoodoo::IsInstalled());

	if (ImGui::Checkbox("Run the game through dgVoodoo", &enabled))
	{
		DgVoodoo::SetEnabled(enabled);

		if (enabled)
			Dxvk::SetEnabled(false);
	}

	ImGui::EndDisabled();
	ImGui::SameLine();

	if (ImGui::Button("Pick the dgVoodoo folder...") && !g_dgVoodooDialog.IsRunning())
		g_dgVoodooDialog.BeginFolder("Pick the folder you extracted dgVoodoo2 into");

	Help("Download dgVoodoo2 yourself and extract it. Press \"Pick the dgVoodoo folder...\", tick "
		"the box and restart the game. Every change here needs a restart.\n\n"
		"The mod copies the dlls into UNI2-IM\\dgVoodoo and writes dgVoodoo.conf from these "
		"options. Nothing in the game folder is touched.\n\n"
		"Forcing texture filtering or anti-aliasing can break the character colours. Keep them on "
		"the game's own or off unless you know you want them.\n\n"
		"Only one Direct3D wrapper can run at a time; turning this on turns DXVK off.");

	if (g_dgVoodooNote[0] != 0)
		Muted("%s", g_dgVoodooNote);

	if (DgVoodoo::IsRunning())
		Good("%s", DgVoodoo::StatusText());
	else
		Muted("%s", DgVoodoo::StatusText());

	if (!DgVoodoo::IsEnabled())
		return;

	ImGui::Indent();
	DrawDgVoodooChoices();
	ImGui::Unindent();
}

void GraphicsPanel::DrawDxvk()
{
	ImGui::SeparatorText("DXVK");

	std::string picked;

	if (g_dxvkDialog.TakeResult(picked) && !picked.empty())
		Dxvk::InstallFrom(picked, g_dxvkNote, sizeof(g_dxvkNote));

	bool enabled = Dxvk::IsEnabled();

	ImGui::BeginDisabled(!Dxvk::IsInstalled());

	if (ImGui::Checkbox("Run the game through DXVK", &enabled))
	{
		Dxvk::SetEnabled(enabled);

		if (enabled)
			DgVoodoo::SetEnabled(false);
	}

	ImGui::EndDisabled();
	ImGui::SameLine();

	if (ImGui::Button("Pick the DXVK folder...") && !g_dxvkDialog.IsRunning())
		g_dxvkDialog.BeginFolder("Pick the folder you extracted DXVK into");

	Help("Download DXVK yourself (github.com/doitsujin/dxvk) and extract it. Press \"Pick the DXVK "
		"folder...\", tick the box and restart the game. Every change here needs a restart.\n\n"
		"The mod copies x32\\d3d9.dll into UNI2-IM\\DXVK. Nothing in the game folder is touched. DXVK "
		"translates the game's Direct3D 9 calls to Vulkan, unlike dgVoodoo which goes through "
		"Direct3D 11/12.\n\n"
		"The first few matches after enabling this load slower while DXVK compiles and caches "
		"shaders; it gets faster from there.\n\n"
		"Only one Direct3D wrapper can run at a time; turning this on turns dgVoodoo off.");

	if (g_dxvkNote[0] != 0)
		Muted("%s", g_dxvkNote);

	if (Dxvk::IsRunning())
		Good("%s", Dxvk::StatusText());
	else
		Muted("%s", Dxvk::StatusText());
}
