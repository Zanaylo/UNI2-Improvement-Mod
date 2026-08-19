#include "Game/UiAssets.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/DataArchive.h"

#include <Windows.h>

#include <cstdio>
#include <vector>

namespace {

struct Asset
{
	const char* folder;
	const char* file;
};

// Digits and capitals live on page 4 and the hyphen on page 0, so the other three pages of the
// atlas are 3 MB the HUD would never sample.
const Asset kAssets[] = {
	{ "Font", "NewCezanne-B_22.fnt" },
	{ "Font", "NewCezanne-B_22_0.dds" },
	{ "Font", "NewCezanne-B_22_4.dds" },
	{ "System", "sys_win01.dds" },
};

constexpr int kAssetCount = static_cast<int>(sizeof(kAssets) / sizeof(kAssets[0]));

bool g_ready = false;
char g_status[160] = "not looked for yet";

bool Exists(const char* file)
{
	return GetFileAttributesA(GetModAssetPath(file).c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool Write(const char* file, const std::vector<uint8_t>& data)
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, GetModAssetPath(file).c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const bool ok = fwrite(data.data(), 1, data.size(), handle) == data.size();
	fclose(handle);
	return ok;
}

}

void UiAssets::Ensure()
{
	int present = 0;
	int lifted = 0;

	for (int i = 0; i < kAssetCount; ++i)
	{
		if (Exists(kAssets[i].file))
		{
			++present;
			continue;
		}

		std::vector<uint8_t> data;
		if (!DataArchive::Read(kAssets[i].folder, kAssets[i].file, data))
			continue;

		if (!Write(kAssets[i].file, data))
		{
			LOG("assets: %s came out of the archive but could not be written", kAssets[i].file);
			continue;
		}

		++present;
		++lifted;
	}

	g_ready = present == kAssetCount;

	if (g_ready && lifted > 0)
		sprintf_s(g_status, "%d of %d taken from the game's own data", lifted, kAssetCount);
	else if (g_ready)
		strncpy_s(g_status, "already in the Assets folder", _TRUNCATE);
	else if (!DataArchive::IsAvailable())
		strncpy_s(g_status, "the game's d folder is not beside the DLL", _TRUNCATE);
	else
		sprintf_s(g_status, "only %d of %d found - the meter falls back to flat colours", present,
			kAssetCount);

	LOG("assets: %s", g_status);
}

bool UiAssets::AreReady()
{
	return g_ready;
}

const char* UiAssets::GetStatusText()
{
	return g_status;
}
