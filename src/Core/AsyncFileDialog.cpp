#include "Core/AsyncFileDialog.h"

#include <Windows.h>
#include <commdlg.h>

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
