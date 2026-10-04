#pragma once

#include <functional>
#include <mutex>
#include <string>

class BackgroundJob
{
public:
	explicit BackgroundJob(const char* name);

	bool Start(std::function<bool()> work);

	bool IsBusy() const;
	bool ConsumeFinished();

	int Progress() const;
	void SetProgress(int percent);

	std::string Status() const;
	void SetStatus(const char* text);

private:
	const char* m_name;
	mutable std::mutex m_lock;
	std::string m_status = "idle";
	volatile long m_busy = 0;
	volatile long m_finished = 0;
	volatile long m_progress = 0;
};
