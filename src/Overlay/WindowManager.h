#pragma once

#include "Overlay/WindowContainer/WindowContainer.h"

#include <atomic>
#include <d3d9.h>
#include <memory>

class WindowManager
{
public:
	static WindowManager& GetInstance();

	bool Initialize(HWND window, IDirect3DDevice9* device);
	void Shutdown();

	void Render();
	void OnDeviceLost();
	void OnDeviceReset();

	bool HoldsDeviceResources() const { return m_deviceObjectsValid; }

	bool IsInitialized() const { return m_initialized; }
	bool IsOverlayActive() const { return m_overlayActive; }
	bool HasFocus() const { return m_hasFocus; }
	bool WantsInputCapture() const;
	bool WantsKeyboardCapture() const;

	bool& GetBlockGameMouse() { return m_blockGameMouse; }

	WindowContainer* GetContainer() { return m_container.get(); }

	LRESULT HandleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, bool& outHandled);

	void OpenUpdateNotifier();

private:
	WindowManager() = default;
	~WindowManager() = default;
	WindowManager(const WindowManager&) = delete;
	WindowManager& operator=(const WindowManager&) = delete;

	void HandleHotkeys();
	void AnnounceUpdate();
	void AnnouncePatch();
	void ScaleToBackBuffer();
	bool GetBackBufferScale(float& outX, float& outY) const;
	LPARAM ScaleMousePosition(LPARAM lParam) const;
	void FeedOverlayMouse(float scaleX, float scaleY);
	const char* OverlayMousePosition(float scaleX, float scaleY, ImVec2& outPosition) const;
	bool MonitorMousePosition(POINT cursor, ImVec2& outPosition) const;
	void FeedOverlayButtons() const;
	void ReportOverlayMouse(const ImVec2& position, const char* source);
	void ApplyScale(float scale);
	void ObserveFocus(UINT message, WPARAM wParam);
	void RefreshFocus();
	void NoteFocusEvidence();
	bool FocusEvidenceIsFresh(DWORD now) const;
	bool LooksFocused(DWORD now) const;
	void ReportHotkeyGate(DWORD now);
	void WatchWindowProc();
	void RestoreDeviceObjects();
	void InstallWindowProc(HWND window);
	void RemoveWindowProc();

	bool m_initialized = false;
	bool m_deviceObjectsValid = false;
	bool m_overlayActive = false;
	float m_fontScale = 1.0f;
	ImGuiStyle m_baseStyle;
	float m_appliedScale = 0.0f;
	float m_appliedFontSize = 0.0f;
	bool m_blockGameMouse = false;
	bool m_updateAnnounced = false;
	bool m_hasFocus = true;
	bool m_windowProcWarned = false;
	bool m_windowProcMine = true;
	bool m_focusFromSystem = false;
	bool m_togglePolled = false;
	bool m_toggleMessaged = false;
	bool m_softwareCursor = false;
	bool m_swappedButtons = false;
	std::atomic<DWORD> m_focusEvidenceAt{ 0 };
	DWORD m_focusCheckedAt = 0;
	DWORD m_gateReportedAt = 0;
	DWORD m_mouseReportedAt = 0;
	DWORD m_restoreTriedAt = 0;
	HWND m_window = nullptr;
	IDirect3DDevice9* m_device = nullptr;
	WNDPROC m_originalWndProc = nullptr;

	std::unique_ptr<WindowContainer> m_container;
};
