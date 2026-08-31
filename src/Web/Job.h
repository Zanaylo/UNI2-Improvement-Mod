#pragma once

#include "Web/Http.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace Web
{
	class Job : public Http::Progress
	{
	public:
		enum State
		{
			State_Idle,
			State_Running,
			State_Done,
			State_Failed
		};

		struct Status
		{
			State state;
			int percent;
			uint64_t received;
			uint64_t total;
			char step[96];
			char source[160];
			char error[256];
		};

		using Work = std::function<bool(Job&)>;

		~Job() override;

		bool Start(const char* step, Work work);

		void Cancel();
		bool CancelRequested() const;

		void SetStep(const char* step);
		void SetSource(const std::string& urlOrHost);
		void SetError(const std::string& error);
		void SetIndeterminate();

		bool IsRunning() const;
		void Read(Status& out) const;

		bool TakeCompletion(bool& outSucceeded);

		bool OnProgress(uint64_t received, uint64_t total) override;

	private:
		void Join();
		void Run(Work work);

		mutable std::mutex m_lock;
		std::thread m_worker;
		Status m_status = {};
		std::atomic<bool> m_cancel{ false };
		std::atomic<bool> m_running{ false };
		std::atomic<bool> m_completion{ false };
		std::atomic<bool> m_succeeded{ false };
	};
}
