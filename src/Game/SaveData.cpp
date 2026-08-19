#include "Game/SaveData.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr const char* kMagic = "UNIEL-SaveData ";
constexpr const char* kSteamMagic = "UNIst-SaveData ";

constexpr size_t kMagicBytes = 15;

void* Address(uintptr_t rva)
{
	const uintptr_t address = RvaToAddress(rva);
	if (address == 0)
		return nullptr;

	return reinterpret_cast<void*>(address);
}

bool ReadByte(uintptr_t rva, uint8_t& outValue)
{
	const void* const source = Address(rva);
	if (source == nullptr)
		return false;

	return TryReadMemory(&outValue, source, sizeof(outValue));
}

bool WriteByte(uintptr_t rva, uint8_t value)
{
	void* const target = Address(rva);
	if (target == nullptr)
		return false;

	return TryWriteMemory(target, &value, sizeof(value));
}

bool ReadDword(uintptr_t rva, uint32_t& outValue)
{
	const void* const source = Address(rva);
	if (source == nullptr)
		return false;

	return TryReadMemory(&outValue, source, sizeof(outValue));
}

bool ReadMode(int& outMode)
{
	uint32_t task = 0;
	if (!ReadDword(GameOffsets::kSaveTask, task) || task == 0)
		return false;

	const void* const source =
		reinterpret_cast<const void*>(task + GameOffsets::kSaveTaskMode);

	uint32_t mode = 0;
	if (!IsReadableMemory(source, sizeof(mode)) || !TryReadMemory(&mode, source, sizeof(mode)))
		return false;

	outMode = static_cast<int>(mode);
	return true;
}

bool ReadHeaderValid()
{
	const void* const source = Address(GameOffsets::kSaveHeader);
	if (source == nullptr)
		return false;

	char magic[kMagicBytes + 1] = {};
	if (!TryReadMemory(magic, source, kMagicBytes))
		return false;

	return strcmp(magic, kMagic) == 0 || strcmp(magic, kSteamMagic) == 0;
}

std::string GameDirectory()
{
	wchar_t path[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

	if (length == 0 || length >= MAX_PATH)
		return std::string();

	std::wstring wide(path, length);
	const size_t slash = wide.find_last_of(L'\\');

	if (slash == std::wstring::npos)
		return std::string();

	wide.resize(slash);

	char narrow[MAX_PATH] = {};
	const int written = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, narrow, MAX_PATH,
		nullptr, nullptr);

	return written > 0 ? std::string(narrow) : std::string();
}

}

bool SaveData::Read(State& out)
{
	uint8_t dirty = 0;
	uint8_t enabled = 0;
	uint8_t requested = 0;

	if (!ReadByte(GameOffsets::kSaveNeededFlag, dirty)
		|| !ReadByte(GameOffsets::kSaveEnabled, enabled)
		|| !ReadByte(GameOffsets::kSaveRequest, requested))
	{
		return false;
	}

	uint32_t buffer = 0;
	uint32_t machine = 0;
	uint32_t size = 0;

	ReadDword(GameOffsets::kSaveBuffer, buffer);
	ReadDword(GameOffsets::kSaveState, machine);
	ReadDword(GameOffsets::kSaveTotalSize, size);

	int mode = 0;
	ReadMode(mode);

	out.dirty = dirty != 0;
	out.enabled = enabled != 0;
	out.requested = requested != 0;
	out.buffered = buffer != 0;
	out.mode = mode;
	out.machine = machine;
	out.size = size;
	out.headerValid = ReadHeaderValid();

	return true;
}

bool SaveData::Request()
{
	return WriteByte(GameOffsets::kSaveRequest, 1);
}

bool SaveData::MarkDirty()
{
	return WriteByte(GameOffsets::kSaveNeededFlag, 1);
}

bool SaveData::ListFiles(std::vector<File>& out)
{
	out.clear();

	const std::string root = GameDirectory();
	if (root.empty())
		return false;

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\Save\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || found.cFileName[0] == '.')
			continue;

		File file;
		file.path = root + "\\Save\\" + found.cFileName + "\\SYS-DATA";

		WIN32_FILE_ATTRIBUTE_DATA data = {};
		if (!GetFileAttributesExA(file.path.c_str(), GetFileExInfoStandard, &data))
			continue;

		file.size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
		file.written = (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32)
			| data.ftLastWriteTime.dwLowDateTime;

		out.push_back(file);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	std::sort(out.begin(), out.end(), [](const File& a, const File& b)
	{
		return a.written > b.written;
	});

	return !out.empty();
}
