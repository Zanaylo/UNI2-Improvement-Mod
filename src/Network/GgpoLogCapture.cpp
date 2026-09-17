#include "Network/GgpoLogCapture.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"
#include "Network/NetLog.h"

#include <cstdarg>
#include <cstdio>

namespace {

void* oUdpLog = nullptr;
void* oProtocolLog = nullptr;

bool g_installed = false;
bool g_enabled = false;
char g_status[128] = "off";

void __cdecl HookedUdpLog(void*, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	NetLog::WriteV("ggpo udp | ", format, args);
	va_end(args);
}

void __cdecl HookedProtocolLog(void* protocol, const char* format, ...)
{
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

bool GgpoLogCapture::IsEnabled()
{
	return g_enabled;
}

const char* GgpoLogCapture::StatusText()
{
	return g_status;
}
