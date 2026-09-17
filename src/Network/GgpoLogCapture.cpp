#include "Network/GgpoLogCapture.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"
#include "Network/NetLog.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

void* oUdpLog = nullptr;
void* oProtocolLog = nullptr;

bool g_installed = false;
bool g_enabled = false;
char g_status[128] = "off";

struct Noise
{
	const char* match;
	const char* name;
};

constexpr Noise kNoise[] =
{
	{ "Skipping past frame", "skipped" },
	{ "send game-compressed-input", "sent" },
	{ "recv game-compressed-input", "received" },
	{ "Throwing away pending output", "dropped output" },
	{ "Sending frame", "queued" },
	{ "Network Stats", "stats" },
};

constexpr int kNoiseKinds = static_cast<int>(sizeof(kNoise) / sizeof(kNoise[0]));

volatile LONG g_counted[kNoiseKinds] = {};

int NoiseKind(const char* format)
{
	if (format == nullptr)
		return -1;

	for (int i = 0; i < kNoiseKinds; ++i)
	{
		if (strstr(format, kNoise[i].match) != nullptr)
			return i;
	}

	return -1;
}

bool Swallow(const char* format)
{
	if (NetLog::IsOverBudget())
		return true;

	const int kind = NoiseKind(format);

	if (kind < 0)
		return false;

	InterlockedIncrement(&g_counted[kind]);
	return true;
}

void __cdecl HookedUdpLog(void*, const char* format, ...)
{
	if (Swallow(format))
		return;

	va_list args;
	va_start(args, format);
	NetLog::WriteV("ggpo udp | ", format, args);
	va_end(args);
}

void __cdecl HookedProtocolLog(void* protocol, const char* format, ...)
{
	if (Swallow(format))
		return;

	uint32_t queue = 0;
	TryReadDword(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(protocol) + GameOffsets::kGgpoEndpointQueue),
		queue);

	char prefix[32] = {};
	sprintf_s(prefix, "ggpo udpproto%u | ", queue);

	va_list args;
	va_start(args, format);
	NetLog::WriteV(prefix, format, args);
	va_end(args);
}

void* Target(uintptr_t rva)
{
	const uintptr_t address = RvaToAddress(rva);

	return address != 0 && IsAddressInGameModule(address) ? reinterpret_cast<void*>(address) : nullptr;
}

bool Install()
{
	if (g_installed)
		return true;

	void* const udp = Target(GameOffsets::kFnGgpoUdpLog);
	void* const protocol = Target(GameOffsets::kFnGgpoProtocolLog);

	if (udp == nullptr || protocol == nullptr)
	{
		strncpy_s(g_status, "this game version is not supported", _TRUNCATE);
		return false;
	}

	if (!HookManager::CreateHook(udp, &HookedUdpLog, &oUdpLog, "GGPO udp log") ||
		!HookManager::CreateHook(protocol, &HookedProtocolLog, &oProtocolLog, "GGPO udpproto log"))
	{
		strncpy_s(g_status, "the hooks could not be created", _TRUNCATE);
		return false;
	}

	g_installed = true;
	return true;
}

void Switch(bool enabled)
{
	HookManager::SetHookEnabled(Target(GameOffsets::kFnGgpoUdpLog), enabled);
	HookManager::SetHookEnabled(Target(GameOffsets::kFnGgpoProtocolLog), enabled);
}

}

void GgpoLogCapture::SetEnabled(bool enabled)
{
	if (enabled == g_enabled)
		return;

	if (enabled && !Install())
	{
		NetLog::Write("ggpo log capture could not start: %s", g_status);
		return;
	}

	if (g_installed)
		Switch(enabled);

	g_enabled = enabled;
	strncpy_s(g_status, enabled ? "on, GGPO's own lines go to the network log" : "off", _TRUNCATE);
	NetLog::Write("ggpo log capture %s", enabled ? "on" : "off");
}

void GgpoLogCapture::ReportSecond()
{
	if (!g_enabled)
		return;

	char text[192] = {};
	int used = 0;
	int kinds = 0;

	for (int i = 0; i < kNoiseKinds; ++i)
	{
		const LONG count = InterlockedExchange(&g_counted[i], 0);

		if (count == 0)
			continue;

		++kinds;
		used += _snprintf_s(text + used, sizeof(text) - used, _TRUNCATE, "%s%s %ld",
			used > 0 ? ", " : "", kNoise[i].name, static_cast<long>(count));
	}

	if (kinds == 0)
		return;

	NetLog::Write("ggpo per-frame lines this second: %s", text);
}

bool GgpoLogCapture::IsEnabled()
{
	return g_enabled;
}

const char* GgpoLogCapture::StatusText()
{
	return g_status;
}
