#include "Core/Harness/Harness.h"

#include "Core/Harness/HarnessChannel.h"
#include "Core/Harness/QuietWindow.h"
#include "Core/logger.h"

#include <Windows.h>

bool Harness::IsActive()
{
	static const bool active = [] {
		char value[8] = {};
		const DWORD length = GetEnvironmentVariableA(kEnvironmentVariable, value, sizeof(value));

		return length > 0 && length < sizeof(value) && value[0] == '1';
	}();

	return active;
}

void Harness::InstallEarly()
{
	if (!IsActive())
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
