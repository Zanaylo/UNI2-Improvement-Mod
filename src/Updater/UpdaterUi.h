#pragma once

#include "Web/Http.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace UpdaterUi
{
	enum class Tone
	{
		Info,
		Question,
		Warning,
		Error,
	};

	struct Choice
	{
		Tone tone = Tone::Question;
		std::wstring instruction;
		std::wstring content;
		std::wstring details;
		std::wstring confirm;
		std::wstring decline = L"Close";
	};

	void Tell(Tone tone, const std::wstring& instruction, const std::wstring& content);
	bool Ask(const Choice& choice);

	class Task : public Http::Progress
	{
	public:
		void SetStep(const std::wstring& step);
		void SetError(const std::string& error);
		void Cancel();
		void Finish();

		bool OnProgress(uint64_t received, uint64_t total) override;

		bool IsCancelled() const;
		bool IsFinished() const;

		std::wstring Step() const;
		std::wstring Error() const;
		uint64_t Received() const;
		uint64_t Total() const;

	private:
		mutable std::mutex m_lock;
		std::wstring m_step;
		std::wstring m_error;
		std::atomic<uint64_t> m_received{ 0 };
		std::atomic<uint64_t> m_total{ 0 };
		std::atomic<bool> m_cancelled{ false };
		std::atomic<bool> m_finished{ false };
	};

	bool Run(const std::wstring& instruction, Task& task, const std::function<bool(Task&)>& work);
}
