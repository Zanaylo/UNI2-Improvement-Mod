#include "Overlay/WindowManager.h"

#include "Core/KeyboardCapture.h"
#include "Core/ProcessTuning.h"
#include "Core/info.h"
#include "Core/Hotkeys.h"
#include "Core/interfaces.h"
#include "Core/PadInput.h"
#include "Core/keycodes.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/DeviceHooks.h"
#include "Game/GamePatches.h"
#include "Game/OnlineState.h"
#include "Hooks/InputProbe.h"
#include "Overlay/FrameMeterHud.h"
#include "Overlay/NotificationBar.h"
#include "Overlay/OverlayFont.h"
#include "Palette/PaletteChoice.h"
#include "Training/FrameMeter.h"
#include "Training/FrameStepper.h"
#include "Training/InputLagMeter.h"
#include "Web/UpdateCheck.h"

#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>
#include <imgui_internal.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr float kReferenceFontSize = 13.0f;

WNDPROC g_originalWndProc = nullptr;

bool WantsTextInputThisFrame()
{
	const ImGuiContext* const context = ImGui::GetCurrentContext();
	if (context != nullptr && context->WantTextInputNextFrame == 1)
		return true;

	return ImGui::GetIO().WantTextInput;
}

bool HasClientMousePosition(UINT message)
{
	switch (message)
	{
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
		return true;
	default:
		return false;
	}
}

bool IsMouseMessage(UINT message)
{
	switch (message)
	{
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
		return true;
	default:
		return false;
	}
}

bool IsKeyboardMessage(UINT message)
{
	switch (message)
	{
	case WM_KEYDOWN: case WM_KEYUP:
	case WM_SYSKEYDOWN: case WM_SYSKEYUP:
	case WM_CHAR: case WM_SYSCHAR:
	case WM_DEADCHAR: case WM_SYSDEADCHAR:
	case WM_UNICHAR:
	case WM_IME_CHAR:
	case WM_IME_STARTCOMPOSITION: case WM_IME_COMPOSITION: case WM_IME_ENDCOMPOSITION:
		return true;
	default:
		return false;
	}
}

LRESULT CALLBACK ModWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	bool handled = false;
	const LRESULT result = WindowManager::GetInstance().HandleWindowMessage(window, message, wParam, lParam, handled);

	if (handled)
		return result;

	if (g_originalWndProc != nullptr)
		return CallWindowProcA(g_originalWndProc, window, message, wParam, lParam);

	return DefWindowProcA(window, message, wParam, lParam);
}

}

WindowManager& WindowManager::GetInstance()
{
	static WindowManager instance;
	return instance;
}

bool WindowManager::Initialize(HWND window, IDirect3DDevice9* device)
{
	if (m_initialized)
		return true;

	if (window == nullptr || device == nullptr)
	{
		LOG("WindowManager::Initialize failed: window=0x%p device=0x%p", (void*)window, (void*)device);
		return false;
	}

	m_window = window;
	m_device = device;
	m_blockGameMouse = g_modVals.blockGameMouse;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	OverlayFont::Load();

	ImGui::StyleColorsDark();
	m_fontScale = ImGui::GetStyle().FontScaleMain;
	m_baseStyle = ImGui::GetStyle();
	m_appliedScale = 0.0f;

	if (!ImGui_ImplWin32_Init(window))
	{
		LOG("ImGui_ImplWin32_Init failed");
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplDX9_Init(device))
	{
		LOG("ImGui_ImplDX9_Init failed");
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	m_deviceObjectsValid = true;
	m_container = std::make_unique<WindowContainer>();

	InstallWindowProc(window);
	ProcessTuning::SetWindow(window);

	m_hasFocus = GetForegroundWindow() == window;
	m_initialized = true;

	NotificationBar::Add("%s %s loaded (press %s to open the main window)", UNI2_IM_NAME,
		UNI2_IM_VERSION, GetNameFromVirtualKey(g_modVals.toggleOverlayKey));

	LOG("WindowManager initialized (ImGui %s), window 0x%p, %s in front", IMGUI_VERSION,
		(void*)window, GetForegroundWindow() == window ? "it is" : "something else is");
	return true;
}

void WindowManager::Shutdown()
{
	if (!m_initialized)
		return;

	InputLagMeter::Shutdown();
	FrameMeter::ShutdownTrace();
	ProcessTuning::Shutdown();

	RemoveWindowProc();

	m_container.reset();

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_initialized = false;
	m_deviceObjectsValid = false;
	m_device = nullptr;
	m_window = nullptr;
}

void WindowManager::InstallWindowProc(HWND window)
{
	g_originalWndProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ModWindowProc)));

	m_originalWndProc = g_originalWndProc;
	LOG("Window procedure installed (original 0x%p)", (void*)g_originalWndProc);
}

void WindowManager::RemoveWindowProc()
{
	if (m_window == nullptr || m_originalWndProc == nullptr)
		return;

	SetWindowLongPtrA(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalWndProc));
	g_originalWndProc = nullptr;
	m_originalWndProc = nullptr;
}

LRESULT WindowManager::HandleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, bool& outHandled)
{
	outHandled = false;

	InputProbe::CountWindowMessage(message);
	ObserveFocus(message, wParam);

	if (!m_initialized)
		return 0;

	if (m_overlayActive)
	{
		ImGui_ImplWin32_WndProcHandler(window, message, wParam,
			HasClientMousePosition(message) ? ScaleMousePosition(lParam) : lParam);
	}

	if (m_blockGameMouse && IsMouseMessage(message))
	{
		outHandled = true;
		return 1;
	}

	if (KeyboardCapture::OwnsKeyboard() && IsKeyboardMessage(message))
	{
		outHandled = true;
		return 1;
	}

	if (!m_overlayActive)
		return 0;

	const ImGuiIO& io = ImGui::GetIO();

	if (io.WantCaptureMouse && message == WM_SETCURSOR)
	{
		outHandled = true;
		return 1;
	}

	if (io.WantCaptureMouse && IsMouseMessage(message))
	{
		outHandled = true;
		return 1;
	}

	return 0;
}

bool WindowManager::WantsInputCapture() const
{
	if (!m_initialized || !m_overlayActive)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard || io.WantCaptureMouse;
}

bool WindowManager::WantsKeyboardCapture() const
{
	if (!m_initialized || !m_overlayActive)
		return false;

	return ImGui::GetIO().WantCaptureKeyboard;
}

void WindowManager::ObserveFocus(UINT message, WPARAM wParam)
{
	if (message == WM_ACTIVATEAPP)
	{
		m_hasFocus = wParam != 0;

		if (m_hasFocus)
			ProcessTuning::Reassert();

		return;
	}

	if (message == WM_SETFOCUS)
		ProcessTuning::Reassert();
}

void WindowManager::AnnounceUpdate()
{
	if (m_updateAnnounced || m_container == nullptr || !UpdateCheck::HasNewer())
		return;

	m_updateAnnounced = true;

	IWindow* const window = m_container->GetWindow(WindowType_UpdateNotifier);

	if (window != nullptr)
		window->Open();
}

void WindowManager::AnnouncePatch()
{
	std::string patch;

	if (GamePatches::TakeAnnouncement(patch))
		NotificationBar::Add("%s", patch.c_str());
}

void WindowManager::HandleHotkeys()
{
	if (m_container == nullptr)
		return;


	if (!m_hasFocus)
	{
		if (GetForegroundWindow() != m_window)
			return;

		m_hasFocus = true;
	}

	if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 && IsHotkeyPressed(VK_F1))
	{
		IWindow* debug = m_container->GetWindow(WindowType_Debug);
		if (debug != nullptr)
			debug->Toggle();
	}

	if (Hotkeys::Pressed(Hotkeys::Action_ToggleOverlay))
	{
		IWindow* main = m_container->GetWindow(WindowType_Main);
		if (main != nullptr)
			main->Toggle();
	}

	if (Hotkeys::Pressed(Hotkeys::Action_FreezeFrame))
		FrameStepper::TogglePaused();

	if (Hotkeys::Repeating(Hotkeys::Action_StepForward,
		static_cast<unsigned>(g_modVals.stepRepeatDelayMs),
		static_cast<unsigned>(g_modVals.stepRepeatIntervalMs)))
		FrameStepper::RequestStep(FrameStepper::GetStepSize());

	if (Hotkeys::Pressed(Hotkeys::Action_ToggleHitbox))
	{
		IWindow* hitboxes = m_container->GetWindow(WindowType_HitboxOverlay);
		if (hitboxes != nullptr)
			hitboxes->Toggle();
	}

	if (Hotkeys::Pressed(Hotkeys::Action_ToggleFrameMeter))
		FrameMeterHud::Toggle();

	if (Hotkeys::Pressed(Hotkeys::Action_NextPalette))
		PaletteChoice::Step(PaletteChoice::LocalPlayer(), 1);

	if (Hotkeys::Pressed(Hotkeys::Action_PreviousPalette))
		PaletteChoice::Step(PaletteChoice::LocalPlayer(), -1);
}


bool WindowManager::GetBackBufferScale(float& outX, float& outY) const
{
	outX = 1.0f;
	outY = 1.0f;

	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	if (present.BackBufferWidth == 0 || present.BackBufferHeight == 0 || m_window == nullptr)
		return false;

	RECT client = {};
	if (!GetClientRect(m_window, &client) || client.right <= 0 || client.bottom <= 0)
		return false;

	outX = static_cast<float>(present.BackBufferWidth) / static_cast<float>(client.right);
	outY = static_cast<float>(present.BackBufferHeight) / static_cast<float>(client.bottom);

	return outX != 1.0f || outY != 1.0f;
}


LPARAM WindowManager::ScaleMousePosition(LPARAM lParam) const
{
	float scaleX = 1.0f;
	float scaleY = 1.0f;

	if (!GetBackBufferScale(scaleX, scaleY))
		return lParam;

	const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)) * scaleX);
	const int y = static_cast<int>(static_cast<short>(HIWORD(lParam)) * scaleY);

	return MAKELPARAM(static_cast<short>(x), static_cast<short>(y));
}

void WindowManager::ScaleToBackBuffer()
{
	ImGuiIO& io = ImGui::GetIO();

	float scaleX = 1.0f;
	float scaleY = 1.0f;

	if (!GetBackBufferScale(scaleX, scaleY))
	{
		ApplyScale(g_modVals.uiScale);
		return;
	}

	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();

	RECT client = {};
	POINT cursor = {};

	if (GetClientRect(m_window, &client) && GetCursorPos(&cursor) &&
		ScreenToClient(m_window, &cursor) && cursor.x >= 0 && cursor.y >= 0 &&
		cursor.x < client.right && cursor.y < client.bottom)
	{
		io.AddMousePosEvent(cursor.x * scaleX, cursor.y * scaleY);
	}

	io.DisplaySize.x = static_cast<float>(present.BackBufferWidth);
	io.DisplaySize.y = static_cast<float>(present.BackBufferHeight);

	ApplyScale(g_modVals.uiScale * scaleX);
}

// The style carries every padding, rounding and border in the same units the windows are laid out
// in, so scaling the font alone leaves the text growing inside furniture that did not. Rebuilt from
// the pristine copy each time rather than scaled again, because ScaleAllSizes multiplies in place.
void WindowManager::ApplyScale(float scale)
{
	if (scale <= 0.0f || (scale == m_appliedScale && g_modVals.fontSize == m_appliedFontSize))
		return;

	m_appliedFontSize = g_modVals.fontSize;

	ImGuiStyle& style = ImGui::GetStyle();

	const float text = g_modVals.fontSize / kReferenceFontSize;

	style = m_baseStyle;
	style.ScaleAllSizes(scale * text);
	style.FontScaleMain = m_fontScale * scale;
	style.FontSizeBase = g_modVals.fontSize;

	m_appliedScale = scale;
}

void WindowManager::Render()
{
	PadInput::OnFrame();

	if (!m_initialized || !m_deviceObjectsValid || IsIconic(m_window))
	{
		KeyboardCapture::ReleaseAll();
		return;
	}

	HandleHotkeys();
	AnnounceUpdate();
	AnnouncePatch();

	m_overlayActive = m_container != nullptr && m_container->AnyWindowOpen();

	if (!m_overlayActive)
		KeyboardCapture::ReleaseAll();

	const bool notifying = NotificationBar::HasPending();
	if (!m_overlayActive && !notifying)
		return;


	ImGui::GetIO().MouseDrawCursor = DeviceHooks::GetPresentParameters().Windowed == FALSE;

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ScaleToBackBuffer();
	ImGui::NewFrame();

	KeyboardCapture::DecayKeyCapture();

	if (m_container != nullptr)
		m_container->UpdateAll();

	NotificationBar::Draw();

	ImGui::EndFrame();

	KeyboardCapture::SetTextInputActive(m_overlayActive && WantsTextInputThisFrame());

	ImGui::Render();

	if (SUCCEEDED(m_device->BeginScene()))
	{
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		m_device->EndScene();
	}
}

void WindowManager::OnDeviceLost()
{
	if (!m_initialized)
		return;

	ImGui_ImplDX9_InvalidateDeviceObjects();
	m_deviceObjectsValid = false;
}

void WindowManager::OnDeviceReset()
{
	if (!m_initialized)
		return;

	if (ImGui_ImplDX9_CreateDeviceObjects())
	{
		m_deviceObjectsValid = true;
	}
	else
	{
		LOG("ImGui_ImplDX9_CreateDeviceObjects failed after reset");
	}
}
