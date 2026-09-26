#include "Core/Harness/InjectedKeys.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace {

constexpr int kKeyCount = 256;
constexpr int kWordBits = 32;
constexpr int kWordCount = kKeyCount / kWordBits;

enum Phase
{
	Phase_Idle,
	Phase_Holding,
	Phase_Releasing
};

std::atomic<uint32_t> g_down[kWordCount] = {};
std::atomic<bool> g_busy{ false };

std::mutex g_lock;
std::condition_variable g_drained;
std::deque<InjectedKeys::Step> g_queue;
InjectedKeys::Step g_current;
Phase g_phase = Phase_Idle;
int g_framesLeft = 0;

bool IsValidKey(int virtualKey)
{
	return virtualKey > 0 && virtualKey < kKeyCount;
}

void SetKeys(const std::vector<int>& keys, bool down)
{
	for (int key : keys)
	{
		if (!IsValidKey(key))
			continue;

		const uint32_t bit = 1u << (key % kWordBits);

		if (down)
			g_down[key / kWordBits].fetch_or(bit, std::memory_order_release);
		else
			g_down[key / kWordBits].fetch_and(~bit, std::memory_order_release);
	}
}

void ReleaseEverything()
{
	for (std::atomic<uint32_t>& word : g_down)
		word.store(0, std::memory_order_release);
}

void Finish()
{
	g_phase = Phase_Idle;
	g_busy.store(false, std::memory_order_release);
	g_drained.notify_all();
}

void StartNext()
{
	if (g_queue.empty())
	{
		Finish();
		return;
	}

	g_current = std::move(g_queue.front());
	g_queue.pop_front();

	SetKeys(g_current.keys, true);
	g_phase = Phase_Holding;
	g_framesLeft = g_current.holdFrames > 0 ? g_current.holdFrames : 1;
}

void Advance()
{
	if (g_phase == Phase_Idle)
	{
		StartNext();
		return;
	}

	if (--g_framesLeft > 0)
		return;

	if (g_phase == Phase_Holding)
	{
		SetKeys(g_current.keys, false);
		g_phase = Phase_Releasing;
		g_framesLeft = g_current.gapFrames > 0 ? g_current.gapFrames : 1;
		return;
	}

	StartNext();
}

}

bool InjectedKeys::Play(const std::vector<Step>& steps, DWORD timeoutMs)
{
	std::unique_lock<std::mutex> lock(g_lock);

	g_queue.insert(g_queue.end(), steps.begin(), steps.end());
	g_busy.store(true, std::memory_order_release);

	if (g_drained.wait_for(lock, std::chrono::milliseconds(timeoutMs),
		[] { return !g_busy.load(std::memory_order_acquire); }))
	{
		return true;
	}

	g_queue.clear();
	ReleaseEverything();
	g_phase = Phase_Idle;
	g_busy.store(false, std::memory_order_release);
	return false;
}

void InjectedKeys::OnFrame()
{
	if (!g_busy.load(std::memory_order_acquire))
		return;

	std::lock_guard<std::mutex> lock(g_lock);

	if (g_busy.load(std::memory_order_relaxed))
		Advance();
}

bool InjectedKeys::IsDown(int virtualKey)
{
	if (!IsValidKey(virtualKey))
		return false;

	return (g_down[virtualKey / kWordBits].load(std::memory_order_acquire) & (1u << (virtualKey % kWordBits))) != 0;
}

void InjectedKeys::Merge(BYTE* keyState)
{
	for (int word = 0; word < kWordCount; ++word)
	{
		uint32_t bits = g_down[word].load(std::memory_order_acquire);

		while (bits != 0)
		{
			unsigned long bit = 0;
			_BitScanForward(&bit, bits);
			bits &= bits - 1;

			keyState[word * kWordBits + bit] |= 0x80;
		}
	}
}
