#include "Web/Job.h"

#include "Core/logger.h"

#include <cstring>

namespace Web {

Job::~Job()
{
	Cancel();
	Join();
}

void Job::Join()
{
	if (m_worker.joinable())
		m_worker.join();
}

bool Job::Start(const char* step, Work work)
{
	if (m_running.load())
		return false;

	Join();

	{
		std::lock_guard<std::mutex> guard(m_lock);

		m_status = {};
		m_status.state = State_Running;
		strncpy_s(m_status.step, step != nullptr ? step : "working", _TRUNCATE);
	}

	m_cancel.store(false);
	m_completion.store(false);
	m_succeeded.store(false);
	m_running.store(true);

	m_worker = std::thread(&Job::Run, this, std::move(work));
	return true;
}

void Job::Run(Work work)
{
	const bool ok = work ? work(*this) : false;

	{
		std::lock_guard<std::mutex> guard(m_lock);

		m_status.state = ok ? State_Done : State_Failed;

		if (ok)
			m_status.percent = 100;
		else if (m_status.error[0] == '\0')
			strncpy_s(m_status.error, "it did not finish", _TRUNCATE);
	}

	m_succeeded.store(ok);
	m_completion.store(true);
	m_running.store(false);
}

void Job::Cancel()
{
	m_cancel.store(true);
}

bool Job::CancelRequested() const
{
	return m_cancel.load();
}

void Job::SetStep(const char* step)
{
	std::lock_guard<std::mutex> guard(m_lock);

	strncpy_s(m_status.step, step != nullptr ? step : "", _TRUNCATE);
}

void Job::SetSource(const std::string& urlOrHost)
{
	const std::string host = urlOrHost.find("://") == std::string::npos
		? urlOrHost
		: Http::HostOf(urlOrHost);

	std::lock_guard<std::mutex> guard(m_lock);

	strncpy_s(m_status.source, host.c_str(), _TRUNCATE);
}

void Job::SetError(const std::string& error)
{
	std::lock_guard<std::mutex> guard(m_lock);

	strncpy_s(m_status.error, error.c_str(), _TRUNCATE);
}

void Job::SetIndeterminate()
{
	std::lock_guard<std::mutex> guard(m_lock);

	m_status.received = 0;
	m_status.total = 0;
	m_status.percent = -1;
}

bool Job::IsRunning() const
{
	return m_running.load();
}

void Job::Read(Status& out) const
{
	std::lock_guard<std::mutex> guard(m_lock);

	out = m_status;
}

bool Job::TakeCompletion(bool& outSucceeded)
{
	if (!m_completion.exchange(false))
		return false;

	Join();

	outSucceeded = m_succeeded.load();
	return true;
}

bool Job::OnProgress(uint64_t received, uint64_t total)
{
	if (m_cancel.load())
		return false;

	std::lock_guard<std::mutex> guard(m_lock);

	m_status.received = received;
	m_status.total = total;
	m_status.percent = total != 0
		? static_cast<int>((received * 100ull) / total)
		: -1;

	return true;
}

}
