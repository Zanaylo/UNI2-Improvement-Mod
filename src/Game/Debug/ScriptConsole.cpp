#include "Game/Debug/ScriptConsole.h"

#include "Core/CodeFingerprint.h"
#include "Core/MeasuredCode.h"
#include "Core/ThreadRole.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"

#include <Windows.h>

#include <cstdint>
#include <string>

namespace {

constexpr DWORD kPipeBytes = 64 * 1024;
constexpr UINT kShiftJis = 932;
constexpr size_t kLongestLine = 4096;

const CodeFingerprint kConsoleCode[] = {
	{ GameOffsets::kSiteScriptConsoleEnabledRead, { 0x83, 0x3D }, 2, GameOffsets::kScriptConsoleEnabled, { 0x00 }, 1 },
	{ GameOffsets::kSiteScriptConsoleHandleRead, { 0xFF, 0x35 }, 2, GameOffsets::kScriptConsoleHandle, {}, 0 },
};

enum class Readiness
{
	Unchecked,
	Ready,
	Refused,
};

Readiness g_readiness = Readiness::Unchecked;
HANDLE g_pipeWrite = nullptr;
volatile LONG g_open = 0;
volatile LONG g_lines = 0;
char g_status[160] = "closed";

bool IsMeasured()
{
	if (g_readiness != Readiness::Unchecked)
		return g_readiness == Readiness::Ready;

	const int mismatch = MeasuredCode::FirstMismatch(kConsoleCode);
	g_readiness = mismatch == MeasuredCode::kAllMatch ? Readiness::Ready : Readiness::Refused;

	if (g_readiness == Readiness::Refused)
	{
		LOG("ScriptConsole: rva 0x%x is not the measured 1.42 code, script output stays hidden",
			static_cast<unsigned>(kConsoleCode[mismatch].siteRva));
	}

	return g_readiness == Readiness::Ready;
}

std::string ToUtf8(const std::string& shiftJis)
{
	const int wideLength = MultiByteToWideChar(kShiftJis, 0, shiftJis.data(), static_cast<int>(shiftJis.size()),
		nullptr, 0);

	if (wideLength <= 0)
		return shiftJis;

	std::wstring wide(static_cast<size_t>(wideLength), L'\0');
	MultiByteToWideChar(kShiftJis, 0, shiftJis.data(), static_cast<int>(shiftJis.size()), &wide[0], wideLength);

	const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, nullptr, 0, nullptr, nullptr);
	std::string out(static_cast<size_t>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, &out[0], length, nullptr, nullptr);

	return out;
}

void Emit(std::string& line)
{
	if (!line.empty() && line.back() == '\r')
		line.pop_back();

	if (line.empty())
		return;

	InterlockedIncrement(&g_lines);
	LOG("[script] %s", ToUtf8(line).c_str());
	line.clear();
}

void Take(const char* data, DWORD length, std::string& line)
{
	for (DWORD i = 0; i < length; ++i)
	{
		if (data[i] == '\n' || line.size() >= kLongestLine)
		{
			Emit(line);

			if (data[i] == '\n')
				continue;
		}

		line.push_back(data[i]);
	}
}

DWORD WINAPI Reader(LPVOID parameter)
{
	ThreadRole::Mark(ThreadRole::Role_Worker);

	const HANDLE pipe = static_cast<HANDLE>(parameter);
	char buffer[1024];
	std::string line;
	DWORD read = 0;

	while (ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr) && read > 0)
		Take(buffer, read, line);

	Emit(line);
	return 0;
}

bool OpenPipe()
{
	if (g_pipeWrite != nullptr)
		return true;

	HANDLE read = nullptr;
	HANDLE write = nullptr;

	if (!CreatePipe(&read, &write, nullptr, kPipeBytes))
		return false;

	const HANDLE thread = CreateThread(nullptr, 0, &Reader, read, 0, nullptr);

	if (thread == nullptr)
	{
		CloseHandle(read);
		CloseHandle(write);
		return false;
	}

	CloseHandle(thread);
	g_pipeWrite = write;
	return true;
}

bool PointConsoleAt(uint32_t enabled, HANDLE handle)
{
	const uint32_t handleValue = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle));

	return TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kScriptConsoleHandle)), &handleValue,
		sizeof(handleValue)) &&
		TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kScriptConsoleEnabled)), &enabled,
			sizeof(enabled));
}

bool StillPointed()
{
	uint32_t enabled = 0;

	return TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kScriptConsoleEnabled)), enabled) &&
		enabled != 0;
}

}

bool ScriptConsole::SetOpen(bool open)
{
	if (open && IsOpen())
		return StillPointed() || PointConsoleAt(1, g_pipeWrite);

	if (open == IsOpen())
		return true;

	if (!IsMeasured())
	{
		strncpy_s(g_status, "this game version is not the measured one, the script output stays hidden", _TRUNCATE);
		return false;
	}

	if (!open)
	{
		PointConsoleAt(0, g_pipeWrite);
		InterlockedExchange(&g_open, 0);
		strncpy_s(g_status, "closed", _TRUNCATE);
		return true;
	}

	if (!OpenPipe() || !PointConsoleAt(1, g_pipeWrite))
	{
		strncpy_s(g_status, "the script output could not be opened", _TRUNCATE);
		LOG("ScriptConsole: %s", g_status);
		return false;
	}

	InterlockedExchange(&g_open, 1);
	strncpy_s(g_status, "open, script output goes to the log as [script] lines", _TRUNCATE);
	LOG("ScriptConsole: %s", g_status);
	return true;
}

bool ScriptConsole::IsOpen()
{
	return InterlockedCompareExchange(&g_open, 0, 0) != 0;
}

long ScriptConsole::Lines()
{
	return InterlockedCompareExchange(&g_lines, 0, 0);
}

const char* ScriptConsole::StatusText()
{
	return g_status;
}
