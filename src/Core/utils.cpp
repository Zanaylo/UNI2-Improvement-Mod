#include "Core/utils.h"

#include "Core/KeyboardCapture.h"
#include "Core/info.h"

#include <Psapi.h>
#include <ShlObj.h>
#include <cstdio>
#include <cctype>
#include <cstring>

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Shlwapi.lib")

namespace {

HMODULE g_modModule = nullptr;
uintptr_t g_gameBase = 0;
size_t g_gameSize = 0;
uint32_t g_gameStamp = 0;
bool g_gameMeasured = false;

void ResolveGameModule()
{
	if (g_gameBase != 0)
		return;

	HMODULE hGame = GetModuleHandleA(nullptr);
	if (!hGame)
		return;

	MODULEINFO info = {};
	if (!GetModuleInformation(GetCurrentProcess(), hGame, &info, sizeof(info)))
		return;

	const uintptr_t base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
	const IMAGE_DOS_HEADER* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	const IMAGE_NT_HEADERS32* const nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);

	if (dos->e_magic == IMAGE_DOS_SIGNATURE && nt->Signature == IMAGE_NT_SIGNATURE)
	{
		g_gameStamp = nt->FileHeader.TimeDateStamp;
		g_gameMeasured = g_gameStamp == UNI2_IM_SUPPORTED_GAME_STAMP &&
			nt->OptionalHeader.SizeOfImage == UNI2_IM_SUPPORTED_GAME_IMAGE_SIZE;
	}

	g_gameSize = info.SizeOfImage;
	g_gameBase = base;
}

}

void SetModModuleHandle(HMODULE hModule)
{
	g_modModule = hModule;
}

HMODULE GetModModuleHandle()
{
	return g_modModule;
}

std::string GetModDirectory()
{
	char path[MAX_PATH] = {};
	if (GetModuleFileNameA(g_modModule, path, MAX_PATH) == 0)
		return std::string();

	std::string full(path);
	const size_t slash = full.find_last_of("\\/");
	if (slash == std::string::npos)
		return std::string();

	return full.substr(0, slash + 1);
}

std::string GetModFilePath(const std::string& fileName)
{
	return GetModDirectory() + fileName;
}

namespace {

const char* const kModFolder = "UNI2-IM";

bool MakeDirectory(const std::string& path)
{
	return CreateDirectoryA(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void MoveStrays(const std::string& root)
{
	const std::string base = GetModDirectory();

	if (!Exists(root + "\\Assets") && Exists(base + "UNI2_IM_assets"))
		MoveFileA((base + "UNI2_IM_assets").c_str(), (root + "\\Assets").c_str());

	if (Exists(base + "UNI2_IM.ini") && !Exists(root + "\\UNI2_IM.ini"))
		MoveFileA((base + "UNI2_IM.ini").c_str(), (root + "\\UNI2_IM.ini").c_str());

	const char* const patterns[] = { "UNI2_IM_*.log", "UNI2_IM_state_*.csv", "UNI2_IM_*.dmp" };

	for (int i = 0; i < static_cast<int>(sizeof(patterns) / sizeof(patterns[0])); ++i)
	{
		WIN32_FIND_DATAA found = {};
		HANDLE search = FindFirstFileA((base + patterns[i]).c_str(), &found);
		if (search == INVALID_HANDLE_VALUE)
			continue;

		do
		{
			if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				continue;

			MoveFileA((base + found.cFileName).c_str(),
				(root + "\\Logs\\" + found.cFileName).c_str());
		}
		while (FindNextFileA(search, &found));

		FindClose(search);
	}
}

}

std::string GetModRootPath(const std::string& fileName)
{
	return GetModDirectory() + kModFolder + "\\" + fileName;
}

std::string GetModAssetPath(const std::string& fileName)
{
	return GetModRootPath("Assets\\") + fileName;
}

std::string GetModPalettePath(const std::string& fileName)
{
	return GetModRootPath("Palettes\\") + fileName;
}

std::string GetModScriptPath(const std::string& fileName)
{
	return GetModRootPath("Scripts\\") + fileName;
}

std::string GetModShaderPath(const std::string& fileName)
{
	return GetModRootPath("Shaders\\") + fileName;
}

std::string GetModDownloadPath(const std::string& fileName)
{
	return GetModRootPath("Downloads\\") + fileName;
}

std::string GetModLogPath(const std::string& fileName)
{
	return GetModRootPath("Logs\\") + fileName;
}

bool CreateDirectoryTree(const std::string& folder)
{
	for (size_t at = folder.find('\\', 3); at != std::string::npos;
		at = folder.find('\\', at + 1))
	{
		MakeDirectory(folder.substr(0, at));
	}

	return MakeDirectory(folder);
}

uint32_t ReadLittle32(const std::vector<uint8_t>& blob, size_t at)
{
	uint32_t value = 0;

	if (at + sizeof(value) <= blob.size())
		memcpy(&value, blob.data() + at, sizeof(value));

	return value;
}

bool CreateModDirectories()
{
	const std::string root = GetModDirectory() + kModFolder;
	if (!MakeDirectory(root))
		return false;

	if (!MakeDirectory(root + "\\Logs"))
		return false;

	MoveStrays(root);

	return MakeDirectory(root + "\\Assets") && MakeDirectory(root + "\\Palettes") &&
		MakeDirectory(root + "\\Downloads") && MakeDirectory(root + "\\Scripts") &&
		MakeDirectory(root + "\\Screens") && MakeDirectory(root + "\\Shaders");
}

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out, size_t minimumSize)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
		return false;

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size <= static_cast<long>(minimumSize))
	{
		fclose(file);
		return false;
	}

	out.resize(static_cast<size_t>(size));
	const size_t read = fread(out.data(), 1, out.size(), file);
	fclose(file);

	return read == out.size();
}

std::string ResourceFileName(const char* name)
{
	if (name == nullptr)
		return std::string();

	std::string text = name;

	if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
		text = text.substr(1, text.size() - 2);

	for (char& character : text)
		character = static_cast<char>(tolower(static_cast<unsigned char>(character)));

	return text;
}

uintptr_t GetGameBaseAddress()
{
	ResolveGameModule();
	return g_gameBase;
}

size_t GetGameModuleSize()
{
	ResolveGameModule();
	return g_gameSize;
}

bool IsMeasuredGameBuild()
{
	ResolveGameModule();
	return g_gameMeasured;
}

uint32_t GetGameBuildStamp()
{
	ResolveGameModule();
	return g_gameStamp;
}

uintptr_t RvaToAddress(uintptr_t rva)
{
	ResolveGameModule();

	if (g_gameBase == 0 || !g_gameMeasured)
		return 0;

	return g_gameBase + rva;
}

bool IsAddressInGameModule(uintptr_t address)
{
	const uintptr_t base = GetGameBaseAddress();
	if (base == 0)
		return false;

	return address >= base && address < base + GetGameModuleSize();
}

bool IsReadableMemory(const void* address, size_t size)
{
	if (address == nullptr)
		return false;

	MEMORY_BASIC_INFORMATION mbi = {};
	if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0)
		return false;

	if (mbi.State != MEM_COMMIT)
		return false;

	const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
		PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
	if ((mbi.Protect & readable) == 0)
		return false;

	if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
		return false;

	const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
	return reinterpret_cast<uintptr_t>(address) + size <= regionEnd;
}

bool TryReadMemory(void* destination, const void* source, size_t size)
{
	if (destination == nullptr || source == nullptr || size == 0)
		return false;

	if (reinterpret_cast<uintptr_t>(source) < 0x10000)
		return false;

	__try
	{
		memcpy(destination, source, size);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

bool TryWriteMemory(void* destination, const void* source, size_t size)
{
	if (destination == nullptr || source == nullptr || size == 0)
		return false;

	if (reinterpret_cast<uintptr_t>(destination) < 0x10000)
		return false;

	__try
	{
		memcpy(destination, source, size);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

bool TryReadDword(const void* source, uint32_t& outValue)
{
	if ((reinterpret_cast<uintptr_t>(source) & 3) != 0)
		return false;

	return TryReadMemory(&outValue, source, sizeof(uint32_t));
}

bool TryWriteDword(void* address, uint32_t value)
{
	if (address == nullptr || (reinterpret_cast<uintptr_t>(address) & 3) != 0)
		return false;

	__try
	{
		*static_cast<uint32_t*>(address) = value;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

bool TryReadUnaligned(const void* source, uint32_t& outValue)
{
	return TryReadMemory(&outValue, source, sizeof(uint32_t));
}

bool TryWriteUnaligned(void* address, uint32_t value)
{
	if (address == nullptr || reinterpret_cast<uintptr_t>(address) < 0x10000)
		return false;

	__try
	{
		memcpy(address, &value, sizeof(value));
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

namespace {

constexpr DWORD kKeyMessageFreshMs = 250;

volatile LONG g_keyMessageTick[256] = {};
volatile LONG g_hotkeyFocus = 1;

bool TakeKeyMessage(int virtualKey)
{
	const DWORD at = static_cast<DWORD>(InterlockedExchange(&g_keyMessageTick[virtualKey], 0));

	return at != 0 && GetTickCount() - at < kKeyMessageFreshMs;
}

bool AsyncKeyDown(int virtualKey)
{
	return HotkeyFocus() && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

}

void NoteHotkeyMessage(int virtualKey)
{
	if (virtualKey <= 0 || virtualKey > 255)
		return;

	const DWORD now = GetTickCount();

	InterlockedExchange(&g_keyMessageTick[virtualKey], static_cast<LONG>(now != 0 ? now : 1));
}

void SetHotkeyFocus(bool focused)
{
	InterlockedExchange(&g_hotkeyFocus, focused ? 1 : 0);
}

bool HotkeyFocus()
{
	return InterlockedCompareExchange(&g_hotkeyFocus, 0, 0) != 0;
}

bool IsHotkeyPressed(int virtualKey)
{
	if (virtualKey <= 0 || virtualKey > 255)
		return false;

	static bool previousState[256] = {};

	const bool isDown = AsyncKeyDown(virtualKey);
	const bool wasDown = previousState[virtualKey];
	const bool messageEdge = TakeKeyMessage(virtualKey);

	previousState[virtualKey] = isDown || messageEdge;

	if (KeyboardCapture::OwnsKeyboard())
		return false;

	return messageEdge || (isDown && !wasDown);
}

bool IsHotkeyHeld(int virtualKey)
{
	if (virtualKey <= 0 || virtualKey > 255 || KeyboardCapture::OwnsKeyboard())
		return false;

	return AsyncKeyDown(virtualKey);
}

bool IsHotkeyRepeating(int virtualKey, unsigned delayMs, unsigned intervalMs)
{
	if (virtualKey <= 0 || virtualKey > 255)
		return false;

	static bool down[256] = {};
	static DWORD nextFire[256] = {};

	const bool isDown = AsyncKeyDown(virtualKey);
	const bool wasDown = down[virtualKey];
	down[virtualKey] = isDown;

	if (!isDown || KeyboardCapture::OwnsKeyboard())
		return false;

	const DWORD now = GetTickCount();

	if (!wasDown)
	{
		nextFire[virtualKey] = now + delayMs;
		return true;
	}

	if (static_cast<int>(now - nextFire[virtualKey]) < 0)
		return false;

	nextFire[virtualKey] = now + intervalMs;
	return true;
}

namespace {

bool StartsWithDirectory(const char* path, const char* directory, UINT length)
{
	if (length == 0 || length >= strlen(path))
		return false;

	const bool separated = directory[length - 1] == '\\' || path[length] == '\\';

	return separated && _strnicmp(path, directory, length) == 0;
}

}

bool IsUnderSystemDirectory(const char* path)
{
	if (path == nullptr)
		return false;

	char system[MAX_PATH] = {};
	char wow64[MAX_PATH] = {};

	const UINT systemLength = GetSystemDirectoryA(system, MAX_PATH);
	const UINT wow64Length = GetSystemWow64DirectoryA(wow64, MAX_PATH);

	return StartsWithDirectory(path, system, systemLength) || StartsWithDirectory(path, wow64, wow64Length);
}

std::string GetSystemDirectoryPath()
{
	char path[MAX_PATH] = {};
	const UINT len = GetSystemDirectoryA(path, MAX_PATH);
	if (len == 0)
		return std::string();

	std::string result(path, len);
	if (!result.empty() && result.back() != '\\')
		result += '\\';

	return result;
}
