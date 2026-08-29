#include "Overlay/WindowContainer/WindowContainer.h"

#include "Core/info.h"
#include "Core/interfaces.h"
#include "Overlay/Window/DebugWindow.h"
#include "Overlay/Window/DummyScriptGuideWindow.h"
#include "Overlay/Window/FrameMeterLegendWindow.h"
#include "Overlay/Window/HitboxLegendWindow.h"
#include "Overlay/Window/PaletteEditorWindow.h"
#include "Overlay/Window/PaletteWindow.h"
#include "Overlay/Window/MusicWindow.h"
#include "Overlay/Window/NetplayWindow.h"
#include "Overlay/Window/PerformanceWindow.h"
#include "Overlay/Window/PlayerControlWindow.h"
#include "Overlay/Window/ThemeWindow.h"
#include "Screens/ScreenDirector.h"
#include "Overlay/Window/UpdateNotifierWindow.h"
#include "Overlay/Window/HitboxOverlay.h"
#include "Overlay/Window/MainWindow.h"

WindowContainer::WindowContainer()
{
	m_windows[WindowType_Main] = std::make_unique<MainWindow>(
		UNI2_IM_NAME " " UNI2_IM_VERSION, true);

	m_windows[WindowType_HitboxOverlay] = std::make_unique<HitboxOverlay>(
		"##hitboxoverlay", false,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoBackground);

	m_windows[WindowType_FrameMeterLegend] = std::make_unique<FrameMeterLegendWindow>(
		"Frame Meter", true);

	m_windows[WindowType_HitboxLegend] = std::make_unique<HitboxLegendWindow>(
		"Hitbox Types", true);

	m_windows[WindowType_DummyScriptGuide] = std::make_unique<DummyScriptGuideWindow>(
		"Dummy script", true);

	m_windows[WindowType_PlayerControl] = std::make_unique<PlayerControlWindow>(
		"Player Control", true);

	m_windows[WindowType_PaletteEditor] = std::make_unique<PaletteEditorWindow>(
		"Palette editor", true);

	m_windows[WindowType_Palette] = std::make_unique<PaletteWindow>("Palette", true);

	m_windows[WindowType_Performance] = std::make_unique<PerformanceWindow>("Performance", true);

	m_windows[WindowType_Netplay] = std::make_unique<NetplayWindow>("Netplay", true);

	m_windows[WindowType_Music] = std::make_unique<MusicWindow>("Music", true);

	if (!ScreenDirector::kOnHold)
		m_windows[WindowType_Theme] = std::make_unique<ThemeWindow>("Theme", true);

	m_windows[WindowType_UpdateNotifier] = std::make_unique<UpdateNotifierWindow>(
		"Update available", true);

	if (g_modVals.memoryDebugEnabled)
		m_windows[WindowType_Debug] = std::make_unique<DebugWindow>("Memory debug", true);
}

void WindowContainer::UpdateAll()
{
	for (auto& entry : m_windows)
		entry.second->Update();
}

bool WindowContainer::AnyWindowOpen() const
{
	for (const auto& entry : m_windows)
	{
		if (entry.second->IsOpen())
			return true;
	}

	return false;
}

IWindow* WindowContainer::GetWindow(WindowType type) const
{
	const auto it = m_windows.find(type);
	if (it == m_windows.end())
		return nullptr;

	return it->second.get();
}
