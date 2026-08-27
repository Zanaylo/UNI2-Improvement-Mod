#include "Core/AsyncFileDialog.h"

#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>

AsyncFileDialog::~AsyncFileDialog()
{
	Join();
}

void AsyncFileDialog::Join()
{
	if (m_worker.joinable())
		m_worker.join();
}

bool AsyncFileDialog::IsRunning() const
{
	return m_running.load(std::memory_order_acquire);
}

bool AsyncFileDialog::TakeResult(std::string& outPath)
{
	if (!m_ready.load(std::memory_order_acquire))
		return false;

	Join();

	m_ready.store(false, std::memory_order_release);
	m_running.store(false, std::memory_order_release);

	if (!m_picked)
		return false;

	outPath = m_path;
	return true;
}

namespace {

enum Pick
{
	Pick_Unavailable,
	Pick_Cancelled,
	Pick_Chosen,
};

Pick PickFolderModern(const char* title, char* out, size_t capacity)
{
	IFileDialog* dialog = nullptr;

	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&dialog))))
	{
		return Pick_Unavailable;
	}

	DWORD options = 0;
	dialog->GetOptions(&options);
	dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

	wchar_t wide[256] = {};

	if (MultiByteToWideChar(CP_UTF8, 0, title, -1, wide, 256) > 0)
		dialog->SetTitle(wide);

	bool picked = false;

	if (SUCCEEDED(dialog->Show(nullptr)))
	{
		IShellItem* item = nullptr;

		if (SUCCEEDED(dialog->GetResult(&item)))
		{
			wchar_t* path = nullptr;

			if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
			{
				picked = WideCharToMultiByte(CP_UTF8, 0, path, -1, out,
					static_cast<int>(capacity), nullptr, nullptr) > 0;
				CoTaskMemFree(path);
			}

			item->Release();
		}
	}

	dialog->Release();
	return picked ? Pick_Chosen : Pick_Cancelled;
}

bool PickFolderLegacy(const char* title, char* out)
{
	BROWSEINFOA browse = {};
	browse.lpszTitle = title;
	browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON;

	LPITEMIDLIST picked = SHBrowseForFolderA(&browse);

	if (picked == nullptr)
		return false;

	const bool ok = SHGetPathFromIDListA(picked, out) != FALSE;
	CoTaskMemFree(picked);
	return ok;
}

}

void AsyncFileDialog::BeginFolder(const char* title)
{
	if (m_running.load(std::memory_order_acquire))
		return;

	Join();

	m_running.store(true, std::memory_order_release);
	m_ready.store(false, std::memory_order_release);
	m_picked = false;
	m_path[0] = 0;

	m_worker = std::thread([this, title]()
	{
		char path[sizeof(m_path)] = {};

		CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		const Pick modern = PickFolderModern(title, path, sizeof(path));

		if (modern == Pick_Chosen ||
			(modern == Pick_Unavailable && PickFolderLegacy(title, path)))
		{
			strncpy_s(m_path, path, _TRUNCATE);
			m_picked = true;
		}

		CoUninitialize();

		m_ready.store(true, std::memory_order_release);
	});
}

void AsyncFileDialog::BeginOpen(const char* title, const char* filter)
{
	if (m_running.load(std::memory_order_acquire))
		return;

	Join();

	m_running.store(true, std::memory_order_release);
	m_ready.store(false, std::memory_order_release);
	m_picked = false;
	m_path[0] = '\0';

	m_worker = std::thread([this, title, filter]()
	{
		char path[sizeof(m_path)] = {};

		OPENFILENAMEA dialog = {};
		dialog.lStructSize = sizeof(dialog);
		dialog.lpstrFilter = filter;
		dialog.lpstrFile = path;
		dialog.nMaxFile = sizeof(path);
		dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		dialog.lpstrTitle = title;

		m_picked = GetOpenFileNameA(&dialog) != 0;

		if (m_picked)
			strncpy_s(m_path, path, _TRUNCATE);

		m_ready.store(true, std::memory_order_release);
	});
}

void AsyncFileDialog::BeginSave(const char* title, const char* filter, const char* defaultName)
{
	if (m_running.load(std::memory_order_acquire))
		return;

	Join();

	m_running.store(true, std::memory_order_release);
	m_ready.store(false, std::memory_order_release);
	m_picked = false;
	strncpy_s(m_path, defaultName != nullptr ? defaultName : "", _TRUNCATE);

	m_worker = std::thread([this, title, filter]()
	{
		char path[sizeof(m_path)] = {};
		strncpy_s(path, m_path, _TRUNCATE);

		OPENFILENAMEA dialog = {};
		dialog.lStructSize = sizeof(dialog);
		dialog.lpstrFilter = filter;
		dialog.lpstrFile = path;
		dialog.nMaxFile = sizeof(path);
		dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		dialog.lpstrTitle = title;
		dialog.lpstrDefExt = "png";

		m_picked = GetSaveFileNameA(&dialog) != 0;

		if (m_picked)
			strncpy_s(m_path, path, _TRUNCATE);

		m_ready.store(true, std::memory_order_release);
	});
}
