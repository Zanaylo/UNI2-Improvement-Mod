#include "Game/SoundpackTransfer.h"

#include "Core/ZipArchive.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmThemes.h"
#include "Game/ModFiles.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

const char* const kReadme =
	"UNI2 Improvement Mod - soundpacks\r\n"
	"=================================\r\n"
	"\r\n"
	"Extract this over the folder that holds uni2.exe, keeping the folder structure, or import it\r\n"
	"from the mod: F1 -> Music -> Open music -> Soundpacks -> Import.\r\n"
	"\r\n"
	"  UNI2-IM/Mods/Bgm/          the tracks and the slot table the game reads at boot\r\n"
	"  UNI2-IM/Mods/grpdat/CSel/  the game's own BGM picker list\r\n"
	"  UNI2-IM/library/           the mod's catalogue - titles, loop points, slot numbers\r\n"
	"  UNI2-IM/Music/             loose folders of your own music, one folder per pack\r\n"
	"  UNI2-IM/Soundpacks/        the packs you can pick in the overlay\r\n"
	"\r\n"
	"You need the mod installed first; this is only the music.\r\n"
	"\r\n"
	"The music is cosmetic. It is not part of the rollback state, so you and your opponent can run\r\n"
	"different packs, or none, with no risk to a match.\r\n";

char g_status[256] = "idle";
volatile long g_busy = 0;
volatile long g_finished = 0;

struct Job
{
	std::string path;
	bool exporting;
};

std::string Combine(const std::string& folder, const char* name)
{
	std::string out = folder;

	if (!out.empty() && out.back() != '\\')
		out.push_back('\\');

	return out + name;
}

void AddFolder(ZipArchive::Writer& zip, const std::string& folder, const std::string& prefix,
	bool compress)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(folder, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.')
			continue;

		const std::string entry = prefix + found.cFileName;

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			AddFolder(zip, Combine(folder, found.cFileName), entry + "/", compress);
			continue;
		}

		zip.AddFile(entry, Combine(folder, found.cFileName), compress);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

bool RunExport(const std::string& path)
{
	ZipArchive::Writer zip;

	if (!zip.Open(path))
	{
		strncpy_s(g_status, zip.StatusText(), _TRUNCATE);
		return false;
	}

	AddFolder(zip, GetModRootPath("Mods\\Bgm"), "UNI2-IM/Mods/Bgm/", false);
	AddFolder(zip, GetModRootPath("Mods\\grpdat"), "UNI2-IM/Mods/grpdat/", true);
	AddFolder(zip, GetModRootPath("library"), "UNI2-IM/library/", true);
	AddFolder(zip, GetModRootPath("Soundpacks"), "UNI2-IM/Soundpacks/", true);
	AddFolder(zip, GetModRootPath("Music"), "UNI2-IM/Music/", false);

	zip.Add("README.txt", reinterpret_cast<const uint8_t*>(kReadme), strlen(kReadme), true);

	if (!zip.Close())
	{
		strncpy_s(g_status, zip.StatusText(), _TRUNCATE);
		return false;
	}

	_snprintf_s(g_status, _TRUNCATE, "exported %d file(s) to %s", zip.Count(), path.c_str());
	LOG("SoundpackTransfer: %s", g_status);
	return true;
}

bool RunImport(const std::string& path)
{
	int files = 0;
	char detail[192] = {};

	const std::string root = GetModRootPath();
	const std::string parent = root.size() > 1
		? root.substr(0, root.find_last_of('\\', root.size() - 2)) : root;

	if (!ZipArchive::Extract(path, parent, files, detail, sizeof(detail)))
	{
		sprintf_s(g_status, "nothing imported: %s", detail);
		LOG("SoundpackTransfer: %s", g_status);
		return false;
	}

	InterlockedExchange(&g_finished, 1);

	sprintf_s(g_status, "%s - restart the game so it reads the new bgm.txt", detail);
	LOG("SoundpackTransfer: %s", g_status);
	return true;
}

DWORD WINAPI Worker(void* parameter)
{
	Job* job = static_cast<Job*>(parameter);

	if (job->exporting)
		RunExport(job->path);
	else
		RunImport(job->path);

	delete job;
	InterlockedExchange(&g_busy, 0);
	return 0;
}

bool Start(const char* path, bool exporting)
{
	if (path == nullptr || path[0] == 0)
		return false;

	if (InterlockedCompareExchange(&g_busy, 1, 0) != 0)
		return false;

	Job* job = new Job();
	job->path = path;
	job->exporting = exporting;

	strncpy_s(g_status, exporting ? "exporting..." : "importing...", _TRUNCATE);

	const HANDLE thread = CreateThread(nullptr, 0, &Worker, job, 0, nullptr);

	if (thread == nullptr)
	{
		delete job;
		InterlockedExchange(&g_busy, 0);
		strncpy_s(g_status, "could not start the transfer", _TRUNCATE);
		return false;
	}

	CloseHandle(thread);
	return true;
}

}

bool SoundpackTransfer::Export(const char* zipPath)
{
	return Start(zipPath, true);
}

bool SoundpackTransfer::Import(const char* zipPath)
{
	return Start(zipPath, false);
}

void SoundpackTransfer::Update()
{
	if (InterlockedCompareExchange(&g_finished, 0, 1) != 1)
		return;

	ModFiles::Rescan();
	BgmLibrary::Load();
	BgmThemes::Reload();
}

bool SoundpackTransfer::IsBusy()
{
	return g_busy != 0;
}

const char* SoundpackTransfer::StatusText()
{
	return g_status;
}
