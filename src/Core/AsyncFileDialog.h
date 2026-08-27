#pragma once

#include <atomic>
#include <string>
#include <thread>

class AsyncFileDialog
{
public:
	~AsyncFileDialog();

	bool IsRunning() const;
	bool TakeResult(std::string& outPath);

	void BeginOpen(const char* title, const char* filter);
	void BeginFolder(const char* title);
	void BeginSave(const char* title, const char* filter, const char* defaultName);

private:
	void Join();

	std::thread m_worker;
	std::atomic<bool> m_running{ false };
	std::atomic<bool> m_ready{ false };
	bool m_picked = false;
	char m_path[512] = {};
};
