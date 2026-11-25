#include "ModLoader/LoaderUi.h"

#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace {

constexpr int kWindowWidth = 1120;
constexpr int kWindowHeight = 740;
constexpr UINT_PTR kBgTimerId = 1;
constexpr UINT kBgTimerIntervalMs = 150;

int g_clientWidth = kWindowWidth;
int g_clientHeight = kWindowHeight;

bool IsRepaintMessage(UINT msg)
{
	switch (msg)
	{
	case WM_MOUSEMOVE:
	case WM_MOUSELEAVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_IME_CHAR:
	case WM_IME_COMPOSITION:
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		return true;
	default:
		return false;
	}
}

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ERASEBKGND:
		return 1;

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			g_clientWidth = LOWORD(lParam);
			g_clientHeight = HIWORD(lParam);
			LoaderUi::OnResize(g_clientWidth, g_clientHeight);
			InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		LoaderUi::RenderToDC(dc, g_clientWidth, g_clientHeight);
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_TIMER:
		if (wParam == kBgTimerId && LoaderUi::TickBackground())
			InvalidateRect(hwnd, nullptr, FALSE);
		return 0;

	case WM_DROPFILES:
	{
		const HDROP drop = reinterpret_cast<HDROP>(wParam);
		const UINT count = DragQueryFileA(drop, 0xFFFFFFFF, nullptr, 0);

		for (UINT i = 0; i < count; ++i)
		{
			char path[MAX_PATH * 2] = {};
			if (DragQueryFileA(drop, i, path, sizeof(path)) > 0)
				LoaderUi::OnDropPath(path);
		}

		DragFinish(drop);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}

	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;

	case WM_DESTROY:
		KillTimer(hwnd, kBgTimerId);
		PostQuitMessage(0);
		return 0;
	}

	LoaderUi::HandleMessage(hwnd, msg, wParam, lParam);

	if (IsRepaintMessage(msg))
		InvalidateRect(hwnd, nullptr, FALSE);

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	SetProcessDPIAware();

	const HICON iconBig = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 48, 48, 0));
	const HICON iconSmall = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, 0));

	WNDCLASSEXW wc = { sizeof(wc), CS_DBLCLKS, WndProc, 0, 0, instance, iconBig,
		LoadCursorA(nullptr, IDC_ARROW), nullptr, nullptr, L"UNI2ModLoaderWindow", iconSmall };
	RegisterClassExW(&wc);

	HWND hwnd = CreateWindowW(wc.lpszClassName, L"UNI2 Mod Loader", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight, nullptr, nullptr, wc.hInstance, nullptr);

	if (hwnd == nullptr)
	{
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		CoUninitialize();
		return 1;
	}

	RECT rc = {};
	GetClientRect(hwnd, &rc);
	g_clientWidth = rc.right > 0 ? rc.right : kWindowWidth;
	g_clientHeight = rc.bottom > 0 ? rc.bottom : kWindowHeight;

	if (!LoaderUi::Initialize(hwnd, g_clientWidth, g_clientHeight))
	{
		DestroyWindow(hwnd);
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		CoUninitialize();
		return 2;
	}

	DragAcceptFiles(hwnd, TRUE);
	SetTimer(hwnd, kBgTimerId, kBgTimerIntervalMs, nullptr);
	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	LoaderUi::Shutdown();
	UnregisterClassW(wc.lpszClassName, wc.hInstance);
	CoUninitialize();
	return 0;
}
