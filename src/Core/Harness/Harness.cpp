#include "Core/Harness/Harness.h"

#include "Core/Harness/HarnessChannel.h"
#include "Core/Harness/QuietWindow.h"
#include "Core/logger.h"

#include <Windows.h>

namespace {

constexpr char kOff = '0';
constexpr char kQuiet = '1';
constexpr char kForeground = '2';

char Mode()
{
	static const char mode = [] {
		char value[8] = {};
		const DWORD length = GetEnvironmentVariableA(Harness::kEnvironmentVariable, value, sizeof(value));

		return length > 0 && length < sizeof(value) ? value[0] : kOff;
	}();

	return mode;
}

}

bool Harness::IsActive()
{
	return Mode() == kQuiet || Mode() == kForeground;
}

void Harness::InstallEarly()
{
	if (Mode() != kQuiet)
		return;

	QuietWindow::Install();
}

void Harness::Start()
{
	if (!IsActive())
		return;

	LOG("[Harness] test harness on: window kept in the background (%s), commands on %s",
		QuietWindow::StatusText(), kPipeName);

	HarnessChannel::Start();
}
