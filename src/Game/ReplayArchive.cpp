#include "Game/ReplayArchive.h"

#include "Core/Deflate.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/ReplayFiles.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr size_t kImageBytes = ReplayFiles::kRecordSize * ReplayFiles::kSlotCount;
constexpr size_t kOffTime = 0x112;
constexpr size_t kOffPayloadSize = 0x124;
constexpr size_t kOffPayload = 0x278;

struct Held
{
	ReplayArchive::Account account;
	std::vector<uint8_t> image;
	bool tried;
	int used;
};

std::vector<Held> g_accounts;
bool g_loaded = false;

std::string SaveFolder()
{
	char exePath[MAX_PATH] = {};

	if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
		return std::string();

	std::string folder(exePath);
	const size_t slash = folder.find_last_of("\\/");

	if (slash == std::string::npos)
		return std::string();

	return folder.substr(0, slash + 1) + "Save\\";
}

std::string Trimmed(const std::string& name)
{
	if (name.size() >= 2 && name.front() == '{' && name.back() == '}')
		return name.substr(1, name.size() - 2);

	return name;
}

std::string CloudAccount(const std::string& folder)
{
	std::vector<uint8_t> blob;

	if (!ReadWholeFile(folder + "steam_autocloud.vdf", blob, 8))
		return std::string();

	const std::string text(reinterpret_cast<const char*>(blob.data()), blob.size());
	const size_t key = text.find("accountid");

	if (key == std::string::npos)
		return std::string();

	const size_t open = text.find('"', text.find('"', key + 9) + 1);

	if (open == std::string::npos)
		return std::string();

	const size_t close = text.find('"', open + 1);

	if (close == std::string::npos)
		return std::string();

	return text.substr(open + 1, close - open - 1);
}

void Sort()
{
	std::stable_sort(g_accounts.begin(), g_accounts.end(),
		[](const Held& left, const Held& right)
		{
			if (left.account.own != right.account.own)
				return left.account.own;

			return left.account.stamp > right.account.stamp;
		});
}

}

void ReplayArchive::Load()
{
	if (g_loaded)
		return;

	g_loaded = true;
	g_accounts.clear();

	const std::string folder = SaveFolder();

	if (folder.empty())
		return;

	const std::string cloud = CloudAccount(folder);

	WIN32_FIND_DATAA find = {};
	const HANDLE handle = FindFirstFileA((folder + "*").c_str(), &find);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || find.cFileName[0] == '.')
			continue;

		const std::string path = folder + find.cFileName + "\\REP-DATA";

		WIN32_FILE_ATTRIBUTE_DATA attributes = {};

		if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attributes))
			continue;

		Held held = {};
		held.account.id = Trimmed(find.cFileName);
		held.account.path = path;
		held.account.stamp =
			(static_cast<uint64_t>(attributes.ftLastWriteTime.dwHighDateTime) << 32) |
			attributes.ftLastWriteTime.dwLowDateTime;
		held.account.own = !cloud.empty() && held.account.id == cloud;
		held.used = -1;

		g_accounts.push_back(std::move(held));
	}
	while (FindNextFileA(handle, &find));

	FindClose(handle);

	if (cloud.empty() && !g_accounts.empty())
	{
		Sort();
		g_accounts.front().account.own = true;
	}

	Sort();

	LOG("replay archive: %d save folder(s), own account is %s",
		static_cast<int>(g_accounts.size()),
		g_accounts.empty() ? "none" : g_accounts.front().account.id.c_str());
}

int ReplayArchive::Count()
{
	Load();
	return static_cast<int>(g_accounts.size());
}

const ReplayArchive::Account* ReplayArchive::Get(int index)
{
	if (index < 0 || index >= Count())
		return nullptr;

	return &g_accounts[index].account;
}

int ReplayArchive::OwnIndex()
{
	for (int i = 0; i < Count(); ++i)
	{
		if (g_accounts[i].account.own)
			return i;
	}

	return g_accounts.empty() ? -1 : 0;
}

int ReplayArchive::IndexOfPath(const std::string& path)
{
	for (int i = 0; i < Count(); ++i)
	{
		if (_stricmp(g_accounts[i].account.path.c_str(), path.c_str()) == 0)
			return i;
	}

	return -1;
}

const uint8_t* ReplayArchive::Image(int index)
{
	if (index < 0 || index >= Count())
		return nullptr;

	Held& held = g_accounts[index];

	if (held.tried)
		return held.image.size() == kImageBytes ? held.image.data() : nullptr;

	held.tried = true;

	std::vector<uint8_t> blob;

	if (!ReadWholeFile(held.account.path, blob, 32))
		return nullptr;

	if (blob.size() == kImageBytes)
	{
		held.image.swap(blob);
	}
	else if (!Deflate::Gunzip(blob.data(), blob.size(), held.image, kImageBytes) ||
		held.image.size() != kImageBytes)
	{
		LOG("replay archive: %s did not decompress to an array", held.account.path.c_str());
		held.image.clear();
		return nullptr;
	}

	LOG("replay archive: read %zu bytes from %s", held.image.size(), held.account.path.c_str());
	return held.image.data();
}

int ReplayArchive::Used(int index)
{
	if (index < 0 || index >= Count())
		return 0;

	Held& held = g_accounts[index];

	if (held.used >= 0)
		return held.used;

	const uint8_t* const image = Image(index);

	if (image == nullptr)
		return 0;

	held.used = 0;

	for (int slot = 0; slot < ReplayFiles::kSlotCount; ++slot)
	{
		const uint8_t* const record = image + slot * ReplayFiles::kRecordSize;

		SYSTEMTIME time = {};
		memcpy(&time, record + kOffTime, sizeof(time));

		uint32_t payload = 0;
		memcpy(&payload, record + kOffPayloadSize, sizeof(payload));

		if (time.wYear != 0 && payload != 0 && kOffPayload + payload <= ReplayFiles::kRecordSize)
			++held.used;
	}

	return held.used;
}

void ReplayArchive::Forget()
{
	g_loaded = false;
	g_accounts.clear();
}
