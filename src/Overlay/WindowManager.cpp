#include "Overlay/WindowManager.h"

#include "Core/KeyboardCapture.h"
#include "Core/ProcessTuning.h"
#include "Network/NetLink.h"
#include "Core/info.h"
#include "Core/Hotkeys.h"
#include "Game/BattleCockpit.h"
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
#include "Overlay/SubtitleHud.h"
#include "D3D9/GraphicsWrapper.h"
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
constexpr DWORD kFocusRecheckMs = 250;
constexpr DWORD kFocusEvidenceMs = 1000;
constexpr DWORD kGateReportMs = 1000;
constexpr DWORD kMouseReportMs = 1000;
constexpr DWORD kRestoreRetryMs = 2000;

WNDPROC g_originalWndProc = nullptr;

DWORD NonZeroTick()
{
	const DWORD now = GetTickCount();
	return now != 0 ? now : 1;
}

bool IsFocusEvidence(UINT message)
{
	switch (message)
	{
	case WM_KEYDOWN: case WM_KEYUP:
	case WM_SYSKEYDOWN: case WM_SYSKEYUP:
	case WM_CHAR:
	case WM_SETFOCUS:
		return true;
	default:
		return false;
	}
}

bool ButtonIsDown(int virtualKey)
{
	return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool ForegroundIsThisProcess()
{
	const HWND foreground = GetForegroundWindow();

	if (foreground == nullptr)
		return false;

	DWORD process = 0;
	GetWindowThreadProcessId(foreground, &process);
	return process == GetCurrentProcessId();
}

void NoteKeyDown(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN)
		return;

	if ((lParam & (1 << 30)) != 0)
		return;

	NoteHotkeyMessage(static_cast<int>(wParam));
}

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
	m_swappedButtons = GetSystemMetrics(SM_SWAPBUTTON) != 0;

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

	m_hasFocus = ForegroundIsThisProcess();
	SetHotkeyFocus(m_hasFocus);
	m_focusCheckedAt = GetTickCount();
	m_initialized = true;

	NotificationBar::Add("%s %s loaded (press %s to open the main window)", UNI2_IM_NAME,
		UNI2_IM_VERSION, GetNameFromVirtualKey(g_modVals.toggleOverlayKey));

	LOG("WindowManager initialized (ImGui %s), window 0x%p, root 0x%p, foreground 0x%p (%s)",
		IMGUI_VERSION, (void*)window, (void*)GetAncestor(window, GA_ROOT),
		(void*)GetForegroundWindow(), m_hasFocus ? "this process" : "another process");
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
	NoteKeyDown(message, wParam, lParam);

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
		m_focusFromSystem = true;
		m_hasFocus = wParam != 0;
		SetHotkeyFocus(m_hasFocus);

		if (!m_hasFocus)
			m_focusEvidenceAt.store(0, std::memory_order_relaxed);

		ProcessTuning::Reassert();
		NetLink::NoteFocusChange(m_hasFocus);
		return;
	}

	if (message == WM_KILLFOCUS)
	{
		m_focusEvidenceAt.store(0, std::memory_order_relaxed);
		return;
	}

	if (message == WM_SETFOCUS)
		ProcessTuning::Reassert();

	if (IsFocusEvidence(message))
		NoteFocusEvidence();
}

void WindowManager::NoteFocusEvidence()
{
	m_focusEvidenceAt.store(NonZeroTick(), std::memory_order_relaxed);

	if (m_hasFocus)
		return;

	m_hasFocus = true;
	SetHotkeyFocus(true);
	LOG("WindowManager: a key reached the game window while the system reported another window in "
		"front, so the game counts as focused");
}

bool WindowManager::FocusEvidenceIsFresh(DWORD now) const
{
	const DWORD at = m_focusEvidenceAt.load(std::memory_order_relaxed);

	return at != 0 && now - at < kFocusEvidenceMs;
}

void WindowManager::WatchWindowProc()
{
	if (m_window == nullptr)
		return;

	const LONG_PTR current = GetWindowLongPtrA(m_window, GWLP_WNDPROC);

	m_windowProcMine = current == reinterpret_cast<LONG_PTR>(&ModWindowProc);

	if (m_windowProcMine || m_windowProcWarned)
		return;

	m_windowProcWarned = true;
	LOG("WindowManager: the window procedure is now 0x%p instead of the mod's, so something else "
		"took it after the mod did", reinterpret_cast<void*>(current));
}

bool WindowManager::LooksFocused(DWORD now) const
{
	if (FocusEvidenceIsFresh(now))
		return true;

	if (m_focusFromSystem)
		return m_hasFocus;

	return ForegroundIsThisProcess();
}

void WindowManager::ReportHotkeyGate(DWORD now)
{
	if (m_overlayActive || now - m_gateReportedAt < kGateReportMs)
		return;

	m_gateReportedAt = now;

	const int key = g_modVals.toggleOverlayKey;

	LOG("[Hotkeys] focus=%d from=%s foreground=0x%p(%s) keyEvidence=%d capture=%d %s=%s wndproc=%s",
		m_hasFocus ? 1 : 0, m_focusFromSystem ? "activation" : "poll",
		static_cast<void*>(GetForegroundWindow()), ForegroundIsThisProcess() ? "ours" : "other",
		FocusEvidenceIsFresh(now) ? 1 : 0, KeyboardCapture::OwnsKeyboard() ? 1 : 0,
		GetNameFromVirtualKey(key), (GetAsyncKeyState(key) & 0x8000) != 0 ? "down" : "up",
		m_windowProcMine ? "the mod's" : "taken");
}

void WindowManager::RefreshFocus()
{
	const DWORD now = GetTickCount();

	if (now - m_focusCheckedAt < kFocusRecheckMs)
		return;

	m_focusCheckedAt = now;

	WatchWindowProc();

	const bool focused = LooksFocused(now);

	SetHotkeyFocus(focused);
	ReportHotkeyGate(now);

	if (focused == m_hasFocus)
		return;

	m_hasFocus = focused;
	LOG("WindowManager: %s", focused ? "the game is in front" : "another program is in front");
}

void WindowManager::RestoreDeviceObjects()
{
	const DWORD now = GetTickCount();

	if (m_deviceObjectsValid || now - m_restoreTriedAt < kRestoreRetryMs)
		return;

	m_restoreTriedAt = now;

	if (m_device == nullptr || m_device->TestCooperativeLevel() != D3D_OK)
		return;

	m_deviceObjectsValid = ImGui_ImplDX9_CreateDeviceObjects();

	LOG("WindowManager: overlay device objects %s", m_deviceObjectsValid ? "restored" :
		"still could not be created");
}

void WindowManager::OpenUpdateNotifier()
{
	if (m_container == nullptr)
		return;

	IWindow* const window = m_container->GetWindow(WindowType_UpdateNotifier);

	if (window != nullptr)
		window->Open();
}

void WindowManager::AnnounceUpdate()
{
	if (m_updateAnnounced || !UpdateCheck::HasNewer())
		return;

	m_updateAnnounced = true;
	OpenUpdateNotifier();
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

	RefreshFocus();

	if (IsHotkeyHeld(VK_CONTROL) && IsHotkeyPressed(VK_F1))
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

	if (Hotkeys::Pressed(Hotkeys::Action_HideHud))
		BattleCockpit::SetHidden(!BattleCockpit::IsHidden());
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
	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();

	if (present.BackBufferWidth == 0 || present.BackBufferHeight == 0)
	{
		ApplyScale(g_modVals.uiScale);
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize.x = static_cast<float>(present.BackBufferWidth);
	io.DisplaySize.y = static_cast<float>(present.BackBufferHeight);

	float scaleX = 1.0f;
	float scaleY = 1.0f;

	GetBackBufferScale(scaleX, scaleY);
	FeedOverlayMouse(scaleX, scaleY);
	ApplyScale(g_modVals.uiScale * scaleX);
}

void WindowManager::FeedOverlayMouse(float scaleX, float scaleY)
{
	if (!m_overlayActive)
		return;

	ImVec2 position = { -1.0f, -1.0f };
	const char* const source = OverlayMousePosition(scaleX, scaleY, position);

	ReportOverlayMouse(position, source);

	if (source == nullptr)
		return;

	ImGui::GetIO().AddMousePosEvent(position.x, position.y);

	if (!m_softwareCursor)
		return;

	FeedOverlayButtons();
}

const char* WindowManager::OverlayMousePosition(float scaleX, float scaleY,
	ImVec2& outPosition) const
{
	POINT cursor = {};

	if (GetCursorPos(&cursor) == 0)
		return nullptr;

	POINT client = cursor;
	RECT bounds = {};

	if (m_window != nullptr && GetClientRect(m_window, &bounds) != 0 && bounds.right > 0 &&
		bounds.bottom > 0 && ScreenToClient(m_window, &client) != 0 && client.x >= 0 &&
		client.y >= 0 && client.x < bounds.right && client.y < bounds.bottom)
	{
		outPosition.x = client.x * scaleX;
		outPosition.y = client.y * scaleY;
		return "the window";
	}

	if (!m_softwareCursor || !HotkeyFocus() || !MonitorMousePosition(cursor, outPosition))
		return nullptr;

	const ImGuiIO& io = ImGui::GetIO();

	outPosition.x = ImClamp(outPosition.x, 0.0f, io.DisplaySize.x - 1.0f);
	outPosition.y = ImClamp(outPosition.y, 0.0f, io.DisplaySize.y - 1.0f);

	return "the monitor";
}

bool WindowManager::MonitorMousePosition(POINT cursor, ImVec2& outPosition) const
{
	const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

	if (monitor == nullptr)
		return false;

	MONITORINFO info = { sizeof(MONITORINFO) };

	if (GetMonitorInfoA(monitor, &info) == 0)
		return false;

	const float width = static_cast<float>(info.rcMonitor.right - info.rcMonitor.left);
	const float height = static_cast<float>(info.rcMonitor.bottom - info.rcMonitor.top);

	if (width <= 0.0f || height <= 0.0f)
		return false;

	const ImGuiIO& io = ImGui::GetIO();

	outPosition.x = (cursor.x - info.rcMonitor.left) * io.DisplaySize.x / width;
	outPosition.y = (cursor.y - info.rcMonitor.top) * io.DisplaySize.y / height;

	return true;
}

void WindowManager::FeedOverlayButtons() const
{
	if (!HotkeyFocus())
		return;

	ImGuiIO& io = ImGui::GetIO();

	io.AddMouseButtonEvent(0, ButtonIsDown(m_swappedButtons ? VK_RBUTTON : VK_LBUTTON));
	io.AddMouseButtonEvent(1, ButtonIsDown(m_swappedButtons ? VK_LBUTTON : VK_RBUTTON));
	io.AddMouseButtonEvent(2, ButtonIsDown(VK_MBUTTON));
}

void WindowManager::ReportOverlayMouse(const ImVec2& position, const char* source)
{
	const DWORD now = GetTickCount();

	if (now - m_mouseReportedAt < kMouseReportMs)
		return;

	m_mouseReportedAt = now;

	const ImGuiIO& io = ImGui::GetIO();
	const HWND foreground = GetForegroundWindow();

	RECT client = {};
	POINT cursor = {};

	GetClientRect(m_window, &client);
	GetCursorPos(&cursor);

	LOG("[Overlay mouse] display=%.0fx%.0f client=%ldx%ld screen=%ld,%ld fed=%.0f,%.0f from=%s "
		"window=0x%p foreground=0x%p(%s) software=%d capture=%d",
		io.DisplaySize.x, io.DisplaySize.y, client.right, client.bottom, cursor.x, cursor.y,
		position.x, position.y, source != nullptr ? source : "nothing",
		static_cast<void*>(m_window), static_cast<void*>(foreground),
		foreground == m_window ? "ours" : "another window", m_softwareCursor ? 1 : 0,
		io.WantCaptureMouse ? 1 : 0);
}

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

namespace {

bool WantsSoftwareCursor()
{
	if (g_modVals.overlayCursor == 1)
		return true;

	if (g_modVals.overlayCursor == 2)
		return false;

	return DeviceHooks::GetPresentParameters().Windowed == FALSE || GraphicsWrapper::IsPresent();
}

}

void WindowManager::Render()
{
	PadInput::OnFrame();

	if (!m_initialized)
	{
		KeyboardCapture::ReleaseAll();
		return;
	}

	HandleHotkeys();
	RestoreDeviceObjects();

	if (!m_deviceObjectsValid || IsIconic(m_window))
	{
		KeyboardCapture::ReleaseAll();
		return;
	}

	AnnounceUpdate();
	AnnouncePatch();

	m_overlayActive = m_container != nullptr && m_container->AnyWindowOpen();

	if (!m_overlayActive)
		KeyboardCapture::ReleaseAll();

	const bool notifying = NotificationBar::HasPending();
	const bool subtitling = SubtitleHud::IsShowing();

	if (!m_overlayActive && !notifying && !subtitling)
		return;


	m_softwareCursor = WantsSoftwareCursor();
	ImGui::GetIO().MouseDrawCursor = m_softwareCursor;

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ScaleToBackBuffer();
	ImGui::NewFrame();

	KeyboardCapture::DecayKeyCapture();

	if (m_container != nullptr)
		m_container->UpdateAll();

	NotificationBar::Draw();
	SubtitleHud::Draw();

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
