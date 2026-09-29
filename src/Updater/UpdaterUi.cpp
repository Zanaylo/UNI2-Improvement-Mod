#include "Updater/UpdaterUi.h"

#include "Core/info.h"
#include "Updater/UpdaterPaths.h"

#include <Windows.h>
#include <CommCtrl.h>

#include <cstdio>
#include <thread>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr const wchar_t* kTitle = L"" UNI2_IM_NAME;
constexpr int kConfirmButton = 100;
constexpr int kDeclineButton = 101;
constexpr int kBarRange = 1000;

PCWSTR IconOf(UpdaterUi::Tone tone)
{
	switch (tone)
	{
	case UpdaterUi::Tone::Warning:
		return TD_WARNING_ICON;
	case UpdaterUi::Tone::Error:
		return TD_ERROR_ICON;
	case UpdaterUi::Tone::Info:
	case UpdaterUi::Tone::Question:
		break;
	}

	return TD_INFORMATION_ICON;
}

std::wstring Megabytes(uint64_t bytes)
{
	wchar_t text[32] = {};
	swprintf_s(text, L"%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
	return text;
}

std::wstring Describe(const UpdaterUi::Task& task)
{
	const uint64_t received = task.Received();
	const uint64_t total = task.Total();

	if (received == 0)
		return task.Step();

	if (total == 0)
		return task.Step() + L"\n" + Megabytes(received);

	return task.Step() + L"\n" + Megabytes(received) + L" of " + Megabytes(total);
}

struct Session
{
	UpdaterUi::Task* task;
	std::wstring shown;
	bool marquee;
};

void ShowProgress(HWND window, Session& session)
{
	const uint64_t total = session.task->Total();
	const bool marquee = total == 0;

	if (marquee != session.marquee)
	{
		SendMessageW(window, TDM_SET_MARQUEE_PROGRESS_BAR, marquee ? TRUE : FALSE, 0);
		SendMessageW(window, TDM_SET_PROGRESS_BAR_MARQUEE, marquee ? TRUE : FALSE, 30);
		SendMessageW(window, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, kBarRange));
		session.marquee = marquee;
	}

	if (!marquee)
	{
		const uint64_t position = session.task->Received() * kBarRange / total;
		SendMessageW(window, TDM_SET_PROGRESS_BAR_POS, static_cast<WPARAM>(position), 0);
	}

	const std::wstring text = Describe(*session.task);

	if (text == session.shown)
		return;

	session.shown = text;
	SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_CONTENT,
		reinterpret_cast<LPARAM>(session.shown.c_str()));
}

HRESULT CALLBACK OnProgressEvent(HWND window, UINT notification, WPARAM wParam, LPARAM,
	LONG_PTR data)
{
	Session& session = *reinterpret_cast<Session*>(data);

	if (notification == TDN_CREATED)
	{
		SendMessageW(window, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 30);
		return S_OK;
	}

	if (notification == TDN_TIMER)
	{
		ShowProgress(window, session);

		if (session.task->IsFinished())
			SendMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0);

		return S_OK;
	}

	if (notification != TDN_BUTTON_CLICKED || static_cast<int>(wParam) != IDCANCEL)
		return S_OK;

	if (session.task->IsFinished())
		return S_OK;

	session.task->Cancel();
	SendMessageW(window, TDM_ENABLE_BUTTON, IDCANCEL, FALSE);
	return S_FALSE;
}

}

void UpdaterUi::Tell(Tone tone, const std::wstring& instruction, const std::wstring& content)
{
	TASKDIALOGCONFIG config = {};
	config.cbSize = sizeof(config);
	config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
	config.dwCommonButtons = TDCBF_OK_BUTTON;
	config.pszWindowTitle = kTitle;
	config.pszMainIcon = IconOf(tone);
	config.pszMainInstruction = instruction.c_str();
	config.pszContent = content.c_str();

	TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
}

bool UpdaterUi::Ask(const Choice& choice)
{
	const TASKDIALOG_BUTTON buttons[] = {
		{ kConfirmButton, choice.confirm.c_str() },
		{ kDeclineButton, choice.decline.c_str() },
	};

	TASKDIALOGCONFIG config = {};
	config.cbSize = sizeof(config);
	config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
	config.pszWindowTitle = kTitle;
	config.pszMainIcon = IconOf(choice.tone);
	config.pszMainInstruction = choice.instruction.c_str();
	config.pszContent = choice.content.c_str();
	config.pszExpandedInformation = choice.details.empty() ? nullptr : choice.details.c_str();
	config.pszExpandedControlText = L"Release notes";
	config.cButtons = 2;
	config.pButtons = buttons;
	config.nDefaultButton = kConfirmButton;

	int pressed = 0;

	if (FAILED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr)))
		return false;

	return pressed == kConfirmButton;
}

void UpdaterUi::Task::SetStep(const std::wstring& step)
{
	std::lock_guard<std::mutex> guard(m_lock);

	m_step = step;
	m_received.store(0);
	m_total.store(0);
}

void UpdaterUi::Task::SetError(const std::string& error)
{
	std::lock_guard<std::mutex> guard(m_lock);
	m_error = UpdaterPaths::Wide(error);
}

void UpdaterUi::Task::Cancel()
{
	m_cancelled.store(true);
}

void UpdaterUi::Task::Finish()
{
	m_finished.store(true);
}

bool UpdaterUi::Task::OnProgress(uint64_t received, uint64_t total)
{
	m_received.store(received);
	m_total.store(total);

	return !m_cancelled.load();
}

bool UpdaterUi::Task::IsCancelled() const
{
	return m_cancelled.load();
}

bool UpdaterUi::Task::IsFinished() const
{
	return m_finished.load();
}

std::wstring UpdaterUi::Task::Step() const
{
	std::lock_guard<std::mutex> guard(m_lock);
	return m_step;
}

std::wstring UpdaterUi::Task::Error() const
{
	std::lock_guard<std::mutex> guard(m_lock);
	return m_error;
}

uint64_t UpdaterUi::Task::Received() const
{
	return m_received.load();
}

uint64_t UpdaterUi::Task::Total() const
{
	return m_total.load();
}

bool UpdaterUi::Run(const std::wstring& instruction, Task& task,
	const std::function<bool(Task&)>& work)
{
	bool succeeded = false;

	std::thread worker([&]()
	{
		succeeded = work(task);
		task.Finish();
	});

	Session session = { &task, std::wstring(), true };

	TASKDIALOGCONFIG config = {};
	config.cbSize = sizeof(config);
	config.dwFlags = TDF_SHOW_MARQUEE_PROGRESS_BAR | TDF_CALLBACK_TIMER |
		TDF_POSITION_RELATIVE_TO_WINDOW;
	config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
	config.pszWindowTitle = kTitle;
	config.pszMainInstruction = instruction.c_str();
	config.pszContent = L" \n ";
	config.pfCallback = &OnProgressEvent;
	config.lpCallbackData = reinterpret_cast<LONG_PTR>(&session);

	if (FAILED(TaskDialogIndirect(&config, nullptr, nullptr, nullptr)))
		task.Cancel();

	worker.join();

	return succeeded && !task.IsCancelled();
}
