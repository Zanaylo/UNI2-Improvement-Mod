#include "Game/BgMipmaps.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/DxtMips.h"

#include <d3d9.h>

#include <cstdio>
#include <cstring>
#include <new>

namespace {

constexpr UINT kOneLevel = 1;
constexpr UINT kFromFile = 0xFFFFFFFD;
constexpr DWORD kSampler = 0;
constexpr const char* kMarker = "mipmaps.txt";
constexpr const char* kExtension = ".dds";

volatile long g_seen = 0;
volatile long g_baked = 0;
volatile long g_told = 0;
volatile long g_bakedMs = 0;
volatile long g_plainMs = 0;

char g_status[224] = "nothing has asked yet";

enum Outcome
{
	Outcome_Skipped,
	Outcome_Baked,
	Outcome_Failed,
};

bool IsDds(const std::string& leaf)
{
	const size_t length = leaf.size();

	return length > 4 && _stricmp(leaf.c_str() + length - 4, kExtension) == 0;
}

std::string Leaf(const std::string& folder)
{
	const size_t slash = folder.find_last_of("\\/");

	return slash == std::string::npos ? folder : folder.substr(slash + 1);
}

std::string MarkerOf(const std::string& folder)
{
	return folder + "\\" + kMarker;
}

bool HeaderWants(const std::string& path)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	uint8_t header[DxtMips::kHeaderBytes] = {};
	const size_t read = fread(header, 1, sizeof(header), handle);
	fclose(handle);

	return read == sizeof(header) && DxtMips::HeaderWants(header);
}

bool WriteReplacing(const std::string& path, const std::vector<uint8_t>& data)
{
	const std::string temporary = path + ".baking";

	FILE* handle = nullptr;

	if (fopen_s(&handle, temporary.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const size_t written = fwrite(data.data(), 1, data.size(), handle);
	fclose(handle);

	if (written == data.size() && MoveFileExA(temporary.c_str(), path.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
	{
		return true;
	}

	DeleteFileA(temporary.c_str());
	return false;
}

int BakeGuarded(std::vector<uint8_t>& data)
{
	try
	{
		return DxtMips::Bake(data);
	}
	catch (const std::bad_alloc&)
	{
		return -1;
	}
}

Outcome BakeFile(const std::string& path)
{
	if (!HeaderWants(path))
		return Outcome_Skipped;

	std::vector<uint8_t> data;

	try
	{
		if (!ReadWholeFile(path, data, DxtMips::kHeaderBytes))
			return Outcome_Failed;
	}
	catch (const std::bad_alloc&)
	{
		return Outcome_Failed;
	}

	const int levels = BakeGuarded(data);

	if (levels < 0)
	{
		LOG("BgMipmaps: %s is too large to bake in the memory left, it keeps one level",
			path.c_str());
		return Outcome_Failed;
	}

	if (levels == 0)
		return Outcome_Skipped;

	return WriteReplacing(path, data) ? Outcome_Baked : Outcome_Failed;
}

void Mark(const std::string& folder, int baked)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, MarkerOf(folder).c_str(), "wb") != 0 || handle == nullptr)
		return;

	fprintf(handle, "Baked = %d\r\n", baked);
	fclose(handle);
}

}

bool BgMipmaps::Enabled()
{
	return g_settings.stageMipmaps != 0;
}

void BgMipmaps::BakeInto(const std::string& leaf, std::vector<uint8_t>& data, Tally& tally)
{
	if (!Enabled() || !IsDds(leaf))
		return;

	const DWORD began = GetTickCount();

	if (BakeGuarded(data) <= 0)
		return;

	++tally.files;
	tally.milliseconds += GetTickCount() - began;
}

int BgMipmaps::BakeFolder(const std::string& folder)
{
	if (!Enabled() || GetFileAttributesA(MarkerOf(folder).c_str()) != INVALID_FILE_ATTRIBUTES)
		return 0;

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*.dds").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return 0;

	const DWORD began = GetTickCount();
	int baked = 0;
	bool complete = true;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		const Outcome outcome = BakeFile(folder + "\\" + found.cFileName);

		baked += outcome == Outcome_Baked ? 1 : 0;
		complete = complete && outcome != Outcome_Failed;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	if (complete)
		Mark(folder, baked);

	if (baked != 0 || !complete)
		LOG("BgMipmaps: %s baked %d file(s) in %lu ms%s", Leaf(folder).c_str(), baked,
			static_cast<unsigned long>(GetTickCount() - began),
			complete ? "" : ", and some could not be rewritten and wait for the next start");

	return baked;
}

void BgMipmaps::Unmark(const std::string& folder)
{
	DeleteFileA(MarkerOf(folder).c_str());
}

bool BgMipmaps::FromFile(const void* data, UINT bytes, UINT& levels)
{
	InterlockedIncrement(&g_seen);

	if (levels != kOneLevel || !Enabled() || !DxtMips::IsBaked(data, bytes))
		return false;

	levels = kFromFile;

	InterlockedIncrement(&g_baked);

	if (InterlockedCompareExchange(&g_told, 1, 0) == 0)
		LOG("BgMipmaps: first baked texture read with the chain it carries, none built at load");

	return true;
}

void BgMipmaps::Assert(IDirect3DDevice9* device)
{
	if (device == nullptr || !Enabled())
		return;

	device->SetSamplerState(kSampler, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
}

void BgMipmaps::Note(DWORD milliseconds, bool fromFile)
{
	InterlockedExchangeAdd(fromFile ? &g_bakedMs : &g_plainMs, static_cast<long>(milliseconds));
}

const char* BgMipmaps::StatusText()
{
	const long seen = InterlockedCompareExchange(&g_seen, 0, 0);
	const long baked = InterlockedCompareExchange(&g_baked, 0, 0);

	if (seen == 0)
		return g_status;

	const long bakedMs = InterlockedCompareExchange(&g_bakedMs, 0, 0);
	const long plainMs = InterlockedCompareExchange(&g_plainMs, 0, 0);

	sprintf_s(g_status, "%ld of %ld texture(s) had mipmaps built in, taking %ld ms of the %ld ms "
		"spent loading textures", baked, seen, bakedMs, bakedMs + plainMs);

	return g_status;
}
