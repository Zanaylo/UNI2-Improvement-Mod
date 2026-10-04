#include "Core/BackgroundJob.h"

#include "Core/logger.h"

#include <Windows.h>

#include <thread>

namespace {

constexpr int kDone = 100;

}

BackgroundJob::BackgroundJob(const char* name)
	: m_name(name)
{
}

bool BackgroundJob::Start(std::function<bool()> work)
{
	if (InterlockedCompareExchange(&m_busy, 1, 0) != 0)
		return false;

	InterlockedExchange(&m_progress, 0);

	try
	{
		std::thread([this, work]() {
			if (work())
				InterlockedExchange(&m_finished, 1);

			InterlockedExchange(&m_progress, kDone);
			InterlockedExchange(&m_busy, 0);
		}).detach();
	}
	catch (...)
	{
		InterlockedExchange(&m_busy, 0);
		SetStatus("the work could not be started");
		return false;
	}

	return true;
}

bool BackgroundJob::IsBusy() const
{
	return m_busy != 0;
}

bool BackgroundJob::ConsumeFinished()
{
	return InterlockedCompareExchange(&m_finished, 0, 1) == 1;
}

int BackgroundJob::Progress() const
{
	return static_cast<int>(m_progress);
}

void BackgroundJob::SetProgress(int percent)
{
	InterlockedExchange(&m_progress, percent);
}

std::string BackgroundJob::Status() const
{
	std::lock_guard<std::mutex> guard(m_lock);
	return m_status;
}

void BackgroundJob::SetStatus(const char* text)
{
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_status = text;
	}

	LOG("%s: %s", m_name, text);
}
