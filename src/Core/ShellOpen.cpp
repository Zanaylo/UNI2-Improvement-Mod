#include "Core/ShellOpen.h"

#include "Core/logger.h"

#include <Windows.h>
#include <ShlObj.h>
#include <exdisp.h>
#include <shldisp.h>

#include <thread>

namespace {

template <typename Interface>
class ComRef
{
public:
	ComRef() = default;

	~ComRef()
	{
		if (m_pointer != nullptr)
			m_pointer->Release();
	}

	ComRef(const ComRef&) = delete;
	ComRef& operator=(const ComRef&) = delete;

	Interface** Put() { return &m_pointer; }
	Interface* operator->() const { return m_pointer; }
	bool IsSet() const { return m_pointer != nullptr; }

private:
	Interface* m_pointer = nullptr;
};

class ComApartment
{
public:
	ComApartment()
		: m_owned(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
	{
	}

	~ComApartment()
	{
		if (m_owned)
			CoUninitialize();
	}

	ComApartment(const ComApartment&) = delete;
	ComApartment& operator=(const ComApartment&) = delete;

private:
	bool m_owned;
};

class BasicString
{
public:
	explicit BasicString(const std::wstring& text)
		: m_value(SysAllocString(text.c_str()))
	{
	}

	~BasicString() { SysFreeString(m_value); }

	BasicString(const BasicString&) = delete;
	BasicString& operator=(const BasicString&) = delete;

	BSTR Get() const { return m_value; }

private:
	BSTR m_value;
};

std::wstring Widen(const std::string& text)
{
	const int length = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
	if (length <= 0)
		return std::wstring();

	std::wstring wide(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, &wide[0], length);
	wide.resize(static_cast<size_t>(length - 1));
	return wide;
}

VARIANT Empty()
{
	VARIANT value;
	VariantInit(&value);
	return value;
}

bool DesktopShell(ComRef<IShellDispatch2>& out)
{
	ComRef<IShellWindows> windows;
	if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(windows.Put()))))
		return false;

	VARIANT location = Empty();
	VARIANT root = Empty();
	long handle = 0;
	ComRef<IDispatch> desktop;

	if (windows->FindWindowSW(&location, &root, SWC_DESKTOP, &handle, SWFO_NEEDDISPATCH, desktop.Put()) != S_OK ||
		!desktop.IsSet())
	{
		return false;
	}

	ComRef<IServiceProvider> provider;
	ComRef<IShellBrowser> browser;
	ComRef<IShellView> view;
	ComRef<IDispatch> background;
	ComRef<IShellFolderViewDual> folderView;
	ComRef<IDispatch> application;

	return SUCCEEDED(desktop->QueryInterface(IID_PPV_ARGS(provider.Put()))) &&
		SUCCEEDED(provider->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(browser.Put()))) &&
		SUCCEEDED(browser->QueryActiveShellView(view.Put())) &&
		SUCCEEDED(view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(background.Put()))) &&
		SUCCEEDED(background->QueryInterface(IID_PPV_ARGS(folderView.Put()))) &&
		SUCCEEDED(folderView->get_Application(application.Put())) &&
		SUCCEEDED(application->QueryInterface(IID_PPV_ARGS(out.Put())));
}

bool OpenFromDesktop(const std::wstring& target)
{
	ComRef<IShellDispatch2> shell;
	if (!DesktopShell(shell))
		return false;

	BasicString file(target);
	BasicString verb(L"open");

	VARIANT arguments = Empty();
	VARIANT directory = Empty();
	VARIANT operation = Empty();
	operation.vt = VT_BSTR;
	operation.bstrVal = verb.Get();
	VARIANT show = Empty();
	show.vt = VT_I4;
	show.lVal = SW_SHOWNORMAL;

	return SUCCEEDED(shell->ShellExecute(file.Get(), arguments, directory, operation, show));
}

void OpenDetached(const std::wstring& target)
{
	ComApartment apartment;

	if (OpenFromDesktop(target))
		return;

	LOG("ShellOpen: the desktop shell did not take the request, opening it from the game instead");
	ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}

void ShellOpen::Open(const std::string& target)
{
	if (target.empty())
		return;

	std::thread(OpenDetached, Widen(target)).detach();
}
