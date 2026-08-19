#include "Core/KeyboardCapture.h"

#include <atomic>

namespace {

constexpr int kHoldFrames = 3;
constexpr int kKeyCaptureHoldFrames = 2;

std::atomic<int> g_textInputHold{ 0 };
std::atomic<int> g_keyCaptureHold{ 0 };

}

void KeyboardCapture::SetTextInputActive(bool active)
{
	if (active)
	{
		g_textInputHold.store(kHoldFrames, std::memory_order_relaxed);
		return;
	}

	const int held = g_textInputHold.load(std::memory_order_relaxed);
	if (held > 0)
		g_textInputHold.store(held - 1, std::memory_order_relaxed);
}

void KeyboardCapture::SetKeyCaptureActive(bool active)
{
	g_keyCaptureHold.store(active ? kKeyCaptureHoldFrames : 0, std::memory_order_relaxed);
}

void KeyboardCapture::DecayKeyCapture()
{
	const int held = g_keyCaptureHold.load(std::memory_order_relaxed);
	if (held > 0)
		g_keyCaptureHold.store(held - 1, std::memory_order_relaxed);
}

void KeyboardCapture::ReleaseAll()
{
	g_textInputHold.store(0, std::memory_order_relaxed);
	g_keyCaptureHold.store(0, std::memory_order_relaxed);
}

bool KeyboardCapture::IsKeyCaptureActive()
{
	return g_keyCaptureHold.load(std::memory_order_relaxed) > 0;
}

bool KeyboardCapture::OwnsKeyboard()
{
	return g_textInputHold.load(std::memory_order_relaxed) > 0 ||
		g_keyCaptureHold.load(std::memory_order_relaxed) > 0;
}
