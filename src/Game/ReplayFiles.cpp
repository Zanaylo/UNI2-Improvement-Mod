#include "Game/ReplayFiles.h"

#include "Core/Deflate.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/SteamNames.h"
#include "Hooks/HookManager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

constexpr char kMagic[8] = { 'U', 'N', 'I', '2', 'I', 'M', 'R', 'P' };
constexpr uint32_t kFileVersionPlain = 1;
constexpr uint32_t kFileVersionDeflate = 2;
constexpr uint32_t kFileVersionNamed = 3;

constexpr size_t kOffVersion = 0x104;
constexpr size_t kOffAuthorId = 0x108;
constexpr size_t kOffTime = 0x112;
constexpr size_t kOffLocked = 0x128;
constexpr size_t kOffPlayer = 0x140;
constexpr size_t kPlayerStride = 0xa0;
constexpr size_t kPlayerChara = 0x08;
constexpr size_t kPlayerTitle = 0x44;
constexpr size_t kTitleBytes = 0x20;
constexpr size_t kPlayerSteamIdBack = 0x08;
constexpr size_t kOffTailTime = 0x7a78;

constexpr size_t kStoredNameBytes = 64;
constexpr int kFallbackVersion = 101;

constexpr int kExportsPerPass = 2;

constexpr int kSettleFrames = 180;

constexpr int kLaunchFrames = 600;

constexpr size_t kFileNameChars = 32;

constexpr size_t kOffPayload = 0x278;
constexpr size_t kOffPayloadSize = 0x124;
constexpr size_t kHeaderBytes = 0x300;

constexpr size_t kImageBytes = ReplayFiles::kRecordSize * ReplayFiles::kSlotCount;

const char* const kShortNames[] = {
	"HYD", "LIN", "WAL", "CAR", "ORI", "GOR", "MER", "VAT", "SET",
	"YUZ", "HIL", "ELT", "NAN", "BYA", "AKA", "CHA", "WAG", "ENK",
	"LON", "TSU", "UZU", "MIK", "KAG", "KUO", "PHO", "OGR", "IZU"
};

constexpr int kCharaCount = static_cast<int>(sizeof(kShortNames) / sizeof(kShortNames[0]));

struct FileHeader
{
	char magic[8];
	uint32_t version;
	uint32_t recordSize;
};

struct NameBlock
{
	char player[2][kStoredNameBytes];
};

char g_status[256] = "";

bool g_autoExport = true;
uint64_t g_seen[ReplayFiles::kSlotCount] = {};
bool g_seeded = false;

std::vector<std::string> g_files;
bool g_filesValid = false;

int g_used = -1;
int g_version = 0;

std::vector<uint8_t> g_disk;
bool g_diskTried = false;
bool g_backedUp = false;
uint64_t g_diskStamp = 0;

typedef void(__fastcall* PlayRecordFn)(int source, void* record);
typedef int(__fastcall* InputBlockFn)(int level);

typedef void(*SampleKeyboardFn)();
SampleKeyboardFn oSampleKeyboard = nullptr;

volatile LONG g_playbackRequest = 0;
std::vector<uint8_t> g_playbackRecord;

enum Session
{
	Session_None,
	Session_Requested,
	Session_Playing,
	Session_After
};

Session g_session = Session_None;
int g_settleFrames = 0;

bool g_escapeArmed = false;

void HookedSampleKeyboard()
{
	oSampleKeyboard();
	ReplayFiles::OnGameFrame();
}

uint8_t* MemoryImage()
{
	const uintptr_t at = RvaToAddress(GameOffsets::kReplayArrayPointer);
	if (at == 0)
		return nullptr;

	uint32_t value = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(at), value) || value == 0)
		return nullptr;

	uint8_t* const buffer = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(value));
	return IsReadableMemory(buffer, ReplayFiles::kRecordSize) ? buffer : nullptr;
}

std::string RepDataPath(uint64_t* outStamp = nullptr)
{
	if (outStamp != nullptr)
		*outStamp = 0;

	char exePath[MAX_PATH] = {};
	if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
		return std::string();

	std::string folder(exePath);
	const size_t slash = folder.find_last_of("\\/");
	if (slash == std::string::npos)
		return std::string();

	folder = folder.substr(0, slash + 1) + "Save\\";

	WIN32_FIND_DATAA find = {};
	const HANDLE handle = FindFirstFileA((folder + "*").c_str(), &find);
	if (handle == INVALID_HANDLE_VALUE)
		return std::string();

	std::string best;
	uint64_t newest = 0;

	do
	{
		if ((find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || find.cFileName[0] == '.')
			continue;

		const std::string candidate = folder + find.cFileName + "\\REP-DATA";

		WIN32_FILE_ATTRIBUTE_DATA attributes = {};
		if (!GetFileAttributesExA(candidate.c_str(), GetFileExInfoStandard, &attributes))
			continue;

		const uint64_t stamp =
			(static_cast<uint64_t>(attributes.ftLastWriteTime.dwHighDateTime) << 32) |
			attributes.ftLastWriteTime.dwLowDateTime;

		if (stamp > newest)
		{
			newest = stamp;
			best = candidate;
		}
	}
	while (FindNextFileA(handle, &find));

	FindClose(handle);

	if (outStamp != nullptr)
		*outStamp = newest;

	return best;
}

bool LoadDisk()
{
	if (g_diskTried)
		return g_disk.size() == kImageBytes;

	g_diskTried = true;

	const std::string path = RepDataPath(&g_diskStamp);
	if (path.empty())
		return false;

	std::vector<uint8_t> blob;
	if (!ReadWholeFile(path, blob, 32))
	{
		g_diskStamp = 0;
		return false;
	}

	if (blob.size() == kImageBytes)
	{
		g_disk.swap(blob);
		return true;
	}

	if (!Deflate::Gunzip(blob.data(), blob.size(), g_disk, kImageBytes) ||
		g_disk.size() != kImageBytes)
	{
		LOG("replay files: %s did not decompress to an array", path.c_str());
		g_disk.clear();
		g_diskStamp = 0;
		return false;
	}

	LOG("replay files: read %zu bytes of replays from %s", g_disk.size(), path.c_str());
	return true;
}

bool ReadSlotAt(int slot, size_t offset, void* out, size_t bytes)
{
	if (slot < 0 || slot >= ReplayFiles::kSlotCount || offset + bytes > ReplayFiles::kRecordSize)
		return false;

	const size_t at = static_cast<size_t>(slot) * ReplayFiles::kRecordSize + offset;

	if (uint8_t* const memory = MemoryImage())
		return TryReadMemory(out, memory + at, bytes);

	if (!LoadDisk())
		return false;

	memcpy(out, g_disk.data() + at, bytes);
	return true;
}

bool WriteSlot(int slot, const void* in, size_t bytes)
{
	if (slot < 0 || slot >= ReplayFiles::kSlotCount || bytes > ReplayFiles::kRecordSize)
		return false;

	const size_t at = static_cast<size_t>(slot) * ReplayFiles::kRecordSize;

	if (uint8_t* const memory = MemoryImage())
	{
		if (!TryWriteMemory(memory + at, in, bytes))
			return false;

		if (g_disk.size() == kImageBytes)
			memcpy(g_disk.data() + at, in, bytes);

		return true;
	}

	if (!LoadDisk())
		return false;

	memcpy(g_disk.data() + at, in, bytes);
	return true;
}

uint64_t TimeKey(const SYSTEMTIME& time)
{
	return (static_cast<uint64_t>(time.wYear) << 48) | (static_cast<uint64_t>(time.wMonth) << 40) |
		(static_cast<uint64_t>(time.wDay) << 32) | (static_cast<uint64_t>(time.wHour) << 24) |
		(static_cast<uint64_t>(time.wMinute) << 16) | (static_cast<uint64_t>(time.wSecond) << 8);
}

size_t PlayerBlock(int player)
{
	return kOffPlayer + static_cast<size_t>(player) * kPlayerStride;
}

std::string ReadTitle(const uint8_t* record, int player)
{
	char raw[kTitleBytes + 1] = {};
	memcpy(raw, record + PlayerBlock(player) + kPlayerTitle, kTitleBytes);
	raw[kTitleBytes] = 0;

	return std::string(raw);
}

uint64_t ReadSteamId(const uint8_t* record, int player)
{
	uint64_t id = 0;
	memcpy(&id, record + PlayerBlock(player) - kPlayerSteamIdBack, sizeof(id));

	return id;
}

std::string ReadStored(const char* raw, size_t bytes)
{
	char text[kStoredNameBytes + 1] = {};
	memcpy(text, raw, bytes < kStoredNameBytes ? bytes : kStoredNameBytes);

	return std::string(text);
}

std::string Sanitise(const std::string& text)
{
	std::string out;

	for (char c : text)
	{
		const unsigned char u = static_cast<unsigned char>(c);

		if (u < 0x20 || u >= 0x80 || strchr("\\/:*?\"<>|()[]{}", c) != nullptr)
			continue;

		const char kept = c == ' ' ? '-' : c;

		if (kept == '-' && !out.empty() && out.back() == '-')
			continue;

		out.push_back(kept);

		if (out.size() >= kFileNameChars)
			break;
	}

	while (!out.empty() && (out.back() == '.' || out.back() == '-'))
		out.pop_back();

	return out;
}

const char* ShortName(int chara)
{
	return chara >= 0 && chara < kCharaCount ? kShortNames[chara] : "???";
}

bool EnsureFolder()
{
	const std::string folder = ReplayFiles::GetFolder();
	return CreateDirectoryA(folder.c_str(), nullptr) != 0 ||
		GetLastError() == ERROR_ALREADY_EXISTS;
}

int NextMatchNumber(const SYSTEMTIME& time)
{
	char prefix[32] = {};
	sprintf_s(prefix, "%04d%02d%02d-", time.wYear, time.wMonth, time.wDay);

	int highest = 0;

	for (const std::string& name : ReplayFiles::ListFiles())
	{
		if (name.compare(0, strlen(prefix), prefix) != 0)
			continue;

		const size_t underscore = name.find('_', strlen(prefix));
		if (underscore == std::string::npos || underscore + 4 > name.size())
			continue;

		const int number = atoi(name.substr(underscore + 1, 3).c_str());
		highest = number > highest ? number : highest;
	}

	return highest + 1;
}

std::string TimePrefix(const SYSTEMTIME& time)
{
	char text[32] = {};

	sprintf_s(text, "%04d%02d%02d-%02d%02d%02d_",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	return text;
}

bool AlreadyExported(const SYSTEMTIME& time)
{
	const std::string prefix = TimePrefix(time);

	for (const std::string& name : ReplayFiles::ListFiles())
	{
		if (name.compare(0, prefix.size(), prefix) == 0)
			return true;
	}

	return false;
}

void RememberFile(const std::string& name)
{
	if (!g_filesValid)
		return;

	g_files.insert(std::lower_bound(g_files.begin(), g_files.end(), name,
		std::greater<std::string>()), name);
}

bool WriteRepData()
{
	const std::string path = RepDataPath();
	if (path.empty())
		return false;

	std::vector<uint8_t> image(kImageBytes);

	if (uint8_t* const memory = MemoryImage())
	{
		if (!TryReadMemory(image.data(), memory, image.size()))
			return false;
	}
	else if (g_disk.size() == kImageBytes)
	{
		memcpy(image.data(), g_disk.data(), image.size());
	}
	else
	{
		return false;
	}

	if (!g_backedUp)
	{
		SYSTEMTIME now = {};
		GetLocalTime(&now);

		char stamp[64] = {};
		sprintf_s(stamp, "REP-DATA.backup-%04d%02d%02d-%02d%02d%02d",
			now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

		if (EnsureFolder())
			CopyFileA(path.c_str(), (ReplayFiles::GetFolder() + stamp).c_str(), FALSE);

		g_backedUp = true;
	}

	std::vector<uint8_t> packed;
	if (!Deflate::Gzip(image.data(), image.size(), packed))
		return false;

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
		return false;

	const bool ok = fwrite(packed.data(), 1, packed.size(), file) == packed.size();
	fclose(file);

	LOG("replay files: rewrote %s, %zu bytes (%s)", path.c_str(), packed.size(),
		ok ? "ok" : "failed");

	return ok;
}

void RefreshStats()
{
	if (g_used >= 0)
		return;

	g_used = 0;
	g_version = 0;

	uint64_t newest = 0;

	for (int slot = 0; slot < ReplayFiles::kSlotCount; ++slot)
	{
		ReplayFiles::Info info;
		if (!ReplayFiles::ReadInfo(slot, info) || !info.used)
			continue;

		++g_used;

		const uint64_t key = TimeKey(info.time);
		if (key > newest)
		{
			newest = key;
			g_version = info.version;
		}
	}
}

}

bool ReplayFiles::IsAvailable()
{
	return MemoryImage() != nullptr || LoadDisk();
}

bool ReplayFiles::IsLive()
{
	return MemoryImage() != nullptr;
}

bool ReplayFiles::ReadInfo(int slot, Info& out)
{
	out = Info();

	uint8_t header[kHeaderBytes] = {};
	if (!ReadSlotAt(slot, 0, header, sizeof(header)))
		return false;

	memcpy(&out.time, header + kOffTime, sizeof(SYSTEMTIME));

	uint32_t payload = 0;
	memcpy(&payload, header + kOffPayloadSize, sizeof(payload));

	out.used = out.time.wYear != 0 && payload != 0 && kOffPayload + payload <= kRecordSize;
	if (!out.used)
		return true;

	uint16_t version = 0;
	memcpy(&version, header + kOffVersion, sizeof(version));
	out.version = version;

	out.locked = header[kOffLocked] == 1;

	for (int player = 0; player < 2; ++player)
	{
		uint32_t chara = 0;
		memcpy(&chara, header + PlayerBlock(player) + kPlayerChara, sizeof(chara));

		out.chara[player] = static_cast<int>(chara);
		out.steamId[player] = ReadSteamId(header, player);
		out.title[player] = ReadTitle(header, player);
		out.name[player] = SteamNames::Resolve(out.steamId[player]);
	}

	return true;
}

std::string ReplayFiles::PlayerName(const Info& info, int player)
{
	if (player < 0 || player > 1)
		return std::string();

	if (!info.name[player].empty())
		return info.name[player];

	return player == 0 ? "P1" : "P2";
}

int ReplayFiles::CountUsed()
{
	RefreshStats();
	return g_used < 0 ? 0 : g_used;
}

int ReplayFiles::CurrentVersion()
{
	RefreshStats();
	return g_version;
}

std::string ReplayFiles::DescribeSlot(const Info& info)
{
	if (!info.used)
		return "empty";

	char text[256] = {};

	sprintf_s(text, "%04d-%02d-%02d %02d:%02d  %s (%s) vs %s (%s)%s",
		info.time.wYear, info.time.wMonth, info.time.wDay, info.time.wHour, info.time.wMinute,
		PlayerName(info, 0).c_str(), ShortName(info.chara[0]),
		PlayerName(info, 1).c_str(), ShortName(info.chara[1]),
		info.locked ? "  [protected]" : "");

	return text;
}

std::string ReplayFiles::FileNameFor(const Info& info, int matchNumber)
{
	const std::string first = Sanitise(PlayerName(info, 0));
	const std::string second = Sanitise(PlayerName(info, 1));

	char text[320] = {};

	sprintf_s(text, "%s%03d_%s(%s)_vs_%s(%s).rep",
		TimePrefix(info.time).c_str(),
		matchNumber,
		first.empty() ? "P1" : first.c_str(), ShortName(info.chara[0]),
		second.empty() ? "P2" : second.c_str(), ShortName(info.chara[1]));

	return text;
}

std::string ReplayFiles::GetFolder()
{
	return GetModRootPath("Replays\\");
}

const std::vector<std::string>& ReplayFiles::ListFiles()
{
	if (g_filesValid)
		return g_files;

	g_files.clear();
	g_filesValid = true;

	WIN32_FIND_DATAA find = {};
	const std::string pattern = GetFolder() + "*.rep";

	const HANDLE handle = FindFirstFileA(pattern.c_str(), &find);
	if (handle == INVALID_HANDLE_VALUE)
		return g_files;

	do
	{
		if ((find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			g_files.push_back(find.cFileName);
	}
	while (FindNextFileA(handle, &find));

	FindClose(handle);

	std::sort(g_files.begin(), g_files.end(), std::greater<std::string>());
	return g_files;
}

void ReplayFiles::Refresh()
{
	g_filesValid = false;
	g_used = -1;
	g_diskTried = false;
	g_diskStamp = 0;
	g_disk.clear();
}

std::string ReplayFiles::Export(int slot)
{
	Info info;
	if (!ReadInfo(slot, info) || !info.used)
	{
		sprintf_s(g_status, "slot %d holds no replay", slot + 1);
		return std::string();
	}

	std::vector<uint8_t> record(kRecordSize);
	if (!ReadSlotAt(slot, 0, record.data(), record.size()))
	{
		sprintf_s(g_status, "slot %d could not be read", slot + 1);
		return std::string();
	}

	std::vector<uint8_t> payload;
	const bool packed = Deflate::Compress(record.data(), record.size(), payload) &&
		payload.size() < record.size();

	if (!packed)
		payload = record;

	if (!EnsureFolder())
	{
		sprintf_s(g_status, "the Replays folder could not be created");
		return std::string();
	}

	const std::string name = FileNameFor(info, NextMatchNumber(info.time));
	const std::string path = GetFolder() + name;

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
	{
		sprintf_s(g_status, "could not write %s", path.c_str());
		return std::string();
	}

	FileHeader header = {};
	memcpy(header.magic, kMagic, sizeof(header.magic));
	header.version = packed ? kFileVersionNamed : kFileVersionPlain;
	header.recordSize = static_cast<uint32_t>(kRecordSize);

	NameBlock names = {};
	for (int player = 0; player < 2; ++player)
		strncpy_s(names.player[player], PlayerName(info, player).c_str(), kStoredNameBytes - 1);

	const bool ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
		(!packed || fwrite(&names, sizeof(names), 1, file) == 1) &&
		fwrite(payload.data(), 1, payload.size(), file) == payload.size();

	fclose(file);

	if (!ok)
	{
		DeleteFileA(path.c_str());
		sprintf_s(g_status, "could not write %s", path.c_str());
		return std::string();
	}

	RememberFile(name);
	return path;
}

int ReplayFiles::ExportAll(std::string& outError)
{
	outError.clear();

	if (!IsAvailable())
	{
		outError = "no replays could be read, from the game or from REP-DATA";
		return 0;
	}

	int written = 0;
	int skipped = 0;

	for (int slot = 0; slot < kSlotCount; ++slot)
	{
		Info info;
		if (!ReadInfo(slot, info) || !info.used)
			continue;

		if (AlreadyExported(info.time))
		{
			++skipped;
			continue;
		}

		if (!Export(slot).empty())
			++written;
	}

	sprintf_s(g_status, "exported %d replays, %d were already in the folder", written, skipped);
	LOG("replay files: %s", g_status);

	return written;
}

int ReplayFiles::FindFreeSlot()
{
	int oldest = -1;
	uint64_t oldestKey = ~0ull;

	for (int slot = 0; slot < kSlotCount; ++slot)
	{
		Info info;
		if (!ReadInfo(slot, info))
			continue;

		if (!info.used)
			return slot;

		if (info.locked)
			continue;

		const uint64_t key = TimeKey(info.time);
		if (key < oldestKey)
		{
			oldestKey = key;
			oldest = slot;
		}
	}

	return oldest;
}

bool ReplayFiles::ReadRecordFile(const std::string& path, std::vector<uint8_t>& out,
	std::string outNames[2], std::string& outError)
{
	out.clear();
	outNames[0].clear();
	outNames[1].clear();

	std::vector<uint8_t> blob;
	if (!ReadWholeFile(path, blob, sizeof(FileHeader)))
	{
		outError = "that file could not be read";
		return false;
	}

	FileHeader header = {};
	memcpy(&header, blob.data(), sizeof(header));

	if (memcmp(header.magic, kMagic, sizeof(kMagic)) != 0)
	{
		out = blob;
		return true;
	}

	if (header.recordSize != kRecordSize)
	{
		outError = "that replay file was written for a different record size";
		return false;
	}

	const uint8_t* payload = blob.data() + sizeof(FileHeader);
	size_t available = blob.size() - sizeof(FileHeader);

	if (header.version == kFileVersionNamed)
	{
		if (available < sizeof(NameBlock))
		{
			outError = "that replay file is truncated";
			return false;
		}

		NameBlock names = {};
		memcpy(&names, payload, sizeof(names));

		for (int player = 0; player < 2; ++player)
			outNames[player] = ReadStored(names.player[player], kStoredNameBytes);

		payload += sizeof(NameBlock);
		available -= sizeof(NameBlock);
	}

	if (header.version == kFileVersionDeflate || header.version == kFileVersionNamed)
	{
		if (!Deflate::Inflate(payload, available, out, kRecordSize) || out.size() != kRecordSize)
		{
			outError = "that replay file is damaged";
			return false;
		}

		return true;
	}

	if (available < kRecordSize)
	{
		outError = "that replay file is truncated";
		return false;
	}

	out.assign(payload, payload + kRecordSize);
	return true;
}

bool ReplayFiles::Import(const std::string& path, std::string& outError)
{
	outError.clear();

	if (!IsAvailable())
	{
		outError = "no replays could be read, from the game or from REP-DATA";
		return false;
	}

	std::vector<uint8_t> record;
	std::string names[2];

	if (!ReadRecordFile(path, record, names, outError))
		return false;

	if (record.size() != kRecordSize)
	{
		outError = "that is a bare replay file - it can be played, but it carries no record header "
			"to put in a slot";
		return false;
	}

	uint16_t incoming = 0;
	memcpy(&incoming, record.data() + kOffVersion, sizeof(incoming));

	const int here = CurrentVersion();

	const int slot = FindFreeSlot();
	if (slot < 0)
	{
		outError = "every slot is full and protected";
		return false;
	}

	if (!WriteSlot(slot, record.data(), record.size()))
	{
		outError = "the replay could not be written into the game";
		return false;
	}

	g_used = -1;

	Info info;
	ReadInfo(slot, info);

	g_seen[slot] = TimeKey(info.time);

	const bool persisted = WriteRepData();

	sprintf_s(g_status, "loaded%s - look for %s",
		persisted ? "" : " (this session only - REP-DATA could not be rewritten)",
		DescribeSlot(info).c_str());

	LOG("replay files: %s", g_status);

	if (here != 0 && incoming != 0 && static_cast<int>(incoming) != here)
	{
		outError = "loaded, but it was recorded on replay format " + std::to_string(incoming) +
			" and this game writes " + std::to_string(here) + " - it will probably desync";
	}

	return true;
}

bool ReplayFiles::CanPlay()
{
	return oSampleKeyboard != nullptr && !GameState::IsInMatch() &&
		RvaToAddress(GameOffsets::kFnPlayReplayRecord) != 0;
}

namespace {

bool BuildRecord(std::vector<uint8_t>& record, std::string& outError)
{
	if (record.size() == ReplayFiles::kRecordSize)
	{
		uint32_t declared = 0;
		memcpy(&declared, record.data() + kOffPayloadSize, sizeof(declared));

		if (declared == 0 || kOffPayload + declared > ReplayFiles::kRecordSize)
		{
			outError = "that replay's length field is out of range";
			return false;
		}

		return true;
	}

	if (record.empty() || record.size() + kOffPayload > ReplayFiles::kRecordSize)
	{
		outError = "that replay file is not a length the game can play";
		return false;
	}

	std::vector<uint8_t> built(ReplayFiles::kRecordSize, 0);

	const uint32_t size = static_cast<uint32_t>(record.size());
	memcpy(built.data() + kOffPayload, record.data(), record.size());
	memcpy(built.data() + kOffPayloadSize, &size, sizeof(size));

	const int known = ReplayFiles::CurrentVersion();
	const uint16_t version = static_cast<uint16_t>(known != 0 ? known : kFallbackVersion);
	memcpy(built.data() + kOffVersion, &version, sizeof(version));

	SYSTEMTIME now = {};
	GetLocalTime(&now);
	memcpy(built.data() + kOffTime, &now, sizeof(now));
	memcpy(built.data() + kOffTailTime, &now, sizeof(now));

	record.swap(built);
	return true;
}

std::string Matchup(const std::vector<uint8_t>& record, const std::string names[2])
{
	std::string text;

	for (int player = 0; player < 2; ++player)
	{
		std::string name = names[player];

		if (name.empty())
			name = SteamNames::Resolve(ReadSteamId(record.data(), player));

		if (name.empty())
			name = player == 0 ? "P1" : "P2";

		text += player == 0 ? name : " vs " + name;
	}

	return text;
}

}

bool ReplayFiles::RequestPlayback(const std::string& path, std::string& outError)
{
	outError.clear();

	if (!CanPlay())
	{
		outError = GameState::IsInMatch()
			? "a match is running - leave it first"
			: "the game is not ready to play a replay";
		return false;
	}

	std::vector<uint8_t> record;
	std::string names[2];

	if (!ReadRecordFile(path, record, names, outError))
		return false;

	if (!BuildRecord(record, outError))
		return false;

	sprintf_s(g_status, "playing %s", Matchup(record, names).c_str());

	g_playbackRecord.swap(record);
	InterlockedExchange(&g_playbackRequest, 1);

	return true;
}

namespace {

uint32_t Peek(uintptr_t rva)
{
	const uintptr_t at = RvaToAddress(rva);

	uint32_t value = 0;
	return at != 0 && TryReadDword(reinterpret_cast<const void*>(at), value) ? value : 0;
}

bool Poke(uintptr_t rva, uint32_t value)
{
	const uintptr_t at = RvaToAddress(rva);
	return at != 0 && TryWriteDword(reinterpret_cast<void*>(at), value);
}

int PeekByte(uintptr_t rva)
{
	const uintptr_t at = RvaToAddress(rva);

	uint8_t value = 0;
	return at != 0 && TryReadMemory(&value, reinterpret_cast<const void*>(at), 1) ? value : -1;
}

void RequestMenu()
{
	Poke(GameOffsets::kSceneResultA, 0);
	Poke(GameOffsets::kSceneResultB, 0);
	Poke(GameOffsets::kSceneId, GameOffsets::kSceneMenu);
	Poke(GameOffsets::kSceneRequest, 1);
	Poke(GameOffsets::kSceneRequestFlag, 1);
}

void LeaveDeadList()
{
	if (!g_escapeArmed)
		return;

	if (Peek(GameOffsets::kSceneId) != GameOffsets::kSceneReplayList)
		return;

	g_escapeArmed = false;

	if (PeekByte(GameOffsets::kReplayListActive) != 0)
		return;

	LOG("replay files: the replay asked for a Replay list with no session behind it, "
		"sending it to the menu instead");
	LOG("replay files: %s", ReplayFiles::DescribeInputState());

	RequestMenu();
}

void EndSession()
{
	g_session = Session_None;
	g_settleFrames = 0;
	g_escapeArmed = false;
}

void StartedPlaying()
{
	g_session = Session_Playing;
}

void StoppedPlaying()
{
	g_session = Session_After;
	g_settleFrames = kSettleFrames;

	LOG("replay files: ended with %s", ReplayFiles::DescribeInputState());
}

void ReleaseStuckInput()
{
	if (g_settleFrames <= 0 || --g_settleFrames > 0)
		return;

	int count = 0;
	int level = 0;
	ReplayFiles::ReadInputBlock(count, level);

	if (level <= 0)
		return;

	LOG("replay files: the playback left the input blocked at level %d, releasing it", level);
	ReplayFiles::ClearInputBlock();
}

void AdvanceSession()
{
	const bool inMatch = GameState::IsInMatch();

	switch (g_session)
	{
	case Session_Requested:
		if (inMatch)
			StartedPlaying();
		else if (--g_settleFrames <= 0)
			EndSession();
		break;

	case Session_Playing:
		LeaveDeadList();

		if (!inMatch)
			StoppedPlaying();
		break;

	case Session_After:
		if (inMatch)
		{
			EndSession();
			break;
		}

		LeaveDeadList();
		ReleaseStuckInput();
		break;

	default:
		break;
	}
}

}

void ReplayFiles::OnGameFrame()
{
	AdvanceSession();

	if (InterlockedExchange(&g_playbackRequest, 0) == 0)
		return;

	if (GameState::IsInMatch())
	{
		sprintf_s(g_status, "a match started before the replay could - nothing was done");
		return;
	}

	const uintptr_t play = RvaToAddress(GameOffsets::kFnPlayReplayRecord);
	if (play == 0 || g_playbackRecord.size() != kRecordSize)
		return;

	g_session = Session_Requested;
	g_settleFrames = kLaunchFrames;
	g_escapeArmed = true;

	reinterpret_cast<PlayRecordFn>(play)(GameOffsets::kReplaySourceList,
		g_playbackRecord.data());

	LOG("replay files: %s", g_status);
	LOG("replay files: launched with %s", DescribeInputState());
}

bool ReplayFiles::IsPlaybackSession()
{
	return g_session != Session_None;
}

bool ReplayFiles::Initialize()
{
	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnSampleKeyboard));

	if (target == nullptr || !HookManager::CreateAndEnableHook(target, &HookedSampleKeyboard,
		reinterpret_cast<void**>(&oSampleKeyboard), "SampleKeyboardForReplay"))
	{
		LOG("replay files: could not hook the per-frame update, Start Replay will not work");
		return false;
	}

	return true;
}

void ReplayFiles::SetAutoExport(bool enabled)
{
	g_autoExport = enabled;
}

bool ReplayFiles::GetAutoExport()
{
	return g_autoExport;
}

namespace {

bool SourceIsCurrent()
{
	if (MemoryImage() != nullptr)
		return true;

	uint64_t stamp = 0;
	const std::string path = RepDataPath(&stamp);

	if (path.empty() || stamp == 0)
		return false;

	if (stamp != g_diskStamp)
	{
		g_diskTried = false;
		g_disk.clear();
		g_used = -1;
	}

	return LoadDisk();
}

}

void ReplayFiles::Update()
{
	static int countdown = 0;

	if (--countdown > 0)
		return;

	countdown = 60;

	if (!SourceIsCurrent())
		return;

	const bool seeding = !g_seeded;
	g_seeded = true;

	int budget = kExportsPerPass;

	for (int slot = 0; slot < kSlotCount; ++slot)
	{
		SYSTEMTIME time = {};
		if (!ReadSlotAt(slot, kOffTailTime, &time, sizeof(time)))
			continue;

		const uint64_t live = time.wYear != 0 ? TimeKey(time) : 0;

		if (live == g_seen[slot])
			continue;

		if (!seeding && g_autoExport && live != 0 && !AlreadyExported(time))
		{
			if (budget == 0)
				return;

			--budget;

			const std::string path = Export(slot);
			if (!path.empty())
			{
				sprintf_s(g_status, "saved %s", path.c_str());
				LOG("replay files: %s", g_status);
			}
		}

		g_seen[slot] = live;
		g_used = -1;
	}

	if (seeding)
		LOG("replay files: %d records already in REP-DATA, left for Export all", CountUsed());
}

const char* ReplayFiles::GetStatus()
{
	return g_status;
}

const char* ReplayFiles::DescribeInputState()
{
	static char text[384];

	const auto peek = [](uintptr_t rva) -> uint32_t
	{
		const uintptr_t at = RvaToAddress(rva);

		uint32_t value = 0;
		return at != 0 && TryReadDword(reinterpret_cast<const void*>(at), value) ? value : 0xffffffffu;
	};

	const auto peekByte = [](uintptr_t rva) -> int
	{
		const uintptr_t at = RvaToAddress(rva);

		uint8_t value = 0;
		return at != 0 && TryReadMemory(&value, reinterpret_cast<const void*>(at), 1) ? value : -1;
	};

	int count = 0;
	int level = 0;
	ReadInputBlock(count, level);

	sprintf_s(text,
		"block %d/%d  ext %d/%d  pads %d,%d  mode %u  scene %u  side %u  take %u  kbd %u,%u  "
		"src %d  list %d/%d/%d  rows %08x cur %u buf %08x",
		count, level,
		peekByte(GameOffsets::kExternalInputFlag), peekByte(GameOffsets::kExternalInputFlag + 1),
		static_cast<int>(peek(GameOffsets::kInputPadSlotP1)),
		static_cast<int>(peek(GameOffsets::kInputPadSlotP2)),
		peek(GameOffsets::kBattleMode), peek(0x596a84), peek(GameOffsets::kPlayerSideIndex),
		peek(0x1a64990), peek(GameOffsets::kPadPortIsKeyboard),
		peek(GameOffsets::kPadPortIsKeyboard + 4),
		static_cast<int>(peek(GameOffsets::kReplayPlaySource)),
		peekByte(GameOffsets::kReplayListLoaded), peekByte(GameOffsets::kReplayListActive),
		peekByte(GameOffsets::kReplayListLeaving),
		peek(GameOffsets::kReplayRecordTable), peek(GameOffsets::kReplayListCursor),
		peek(GameOffsets::kReplayListBuffer));

	return text;
}

void ReplayFiles::ReadInputBlock(int& outCount, int& outLevel)
{
	outCount = 0;
	outLevel = 0;

	const uintptr_t count = RvaToAddress(GameOffsets::kInputBlockCount);
	const uintptr_t level = RvaToAddress(GameOffsets::kInputBlockLevel);

	uint32_t value = 0;

	if (count != 0 && TryReadDword(reinterpret_cast<const void*>(count), value))
		outCount = static_cast<int>(value);

	if (level != 0 && TryReadDword(reinterpret_cast<const void*>(level), value))
		outLevel = static_cast<int>(value);
}

bool ReplayFiles::ClearInputBlock()
{
	const uintptr_t pop = RvaToAddress(GameOffsets::kFnPopInputBlock);
	const uintptr_t stack = RvaToAddress(GameOffsets::kInputBlockStack);

	if (pop == 0 || stack == 0)
		return false;

	const InputBlockFn release = reinterpret_cast<InputBlockFn>(pop);

	for (int guard = 0; guard < 64; ++guard)
	{
		int count = 0;
		int level = 0;
		ReadInputBlock(count, level);

		if (count <= 0)
			return true;

		uint32_t entry = 0;
		if (!TryReadDword(reinterpret_cast<const void*>(stack), entry))
			return false;

		release(static_cast<int>(entry));
	}

	return false;
}
