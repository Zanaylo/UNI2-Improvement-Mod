#include "Game/VoiceImport.h"

#include "Core/ZipArchive.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/CharaTables.h"
#include "Game/ModFiles.h"
#include "Game/SoundPacks.h"
#include "Game/VoiceMap.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

struct Build
{
	const char* exe;
	const char* tag;
	const char* title;
};

constexpr Build kBuilds[] = {
	{ "UNIclr.exe", "UNI cl-r", "UNDER NIGHT IN-BIRTH Exe:Late[cl-r]" },
	{ "UNIst.exe", "UNI st", "UNDER NIGHT IN-BIRTH Exe:Late[st]" },
	{ "UNIEL.exe", "UNI", "UNDER NIGHT IN-BIRTH Exe:Late" },
};

struct Job
{
	std::string folder;
	int chara;
};

char g_status[256] = "idle";
char g_pack[128] = {};
volatile long g_busy = 0;
volatile long g_finished = 0;
volatile long g_progress = 0;
volatile long g_chara = -1;

std::string Combine(const std::string& folder, const std::string& name)
{
	if (folder.empty())
		return name;

	std::string out = folder;

	if (out.back() != '\\')
		out.push_back('\\');

	return out + name;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

const Build* BuildFor(const std::string& folder)
{
	for (const Build& build : kBuilds)
	{
		if (Exists(Combine(folder, build.exe)))
			return &build;
	}

	return nullptr;
}

void RemoveTree(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(folder, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.')
			continue;

		const std::string child = Combine(folder, found.cFileName);

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			RemoveTree(child);
		else
			DeleteFileA(child.c_str());
	}
	while (FindNextFileA(search, &found));

	FindClose(search);
	RemoveDirectoryA(folder.c_str());
}

bool WriteWhole(const std::string& path, const uint8_t* data, size_t size)
{
	ZipArchive::MakeFolders(path);

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const bool ok = size == 0 || fwrite(data, 1, size, handle) == size;
	fclose(handle);

	return ok;
}

void WritePackFile(const std::string& root, int chara, const Build& build)
{
	char text[512] = {};

	sprintf_s(text, "[Pack]\r\nName      = %s, %s\r\nAuthor    = \r\nSource    = %s\r\n"
		"Character = %d\r\n", CharaTables::Name(chara), build.tag, build.title, chara);

	WriteWhole(Combine(root, "pack.ini"), reinterpret_cast<const uint8_t*>(text), strlen(text));
}

std::string PackFolder(int chara, const Build& build)
{
	char id[128] = {};
	sprintf_s(id, "%s - %s", CharaTables::Name(chara), build.tag);

	strncpy_s(g_pack, id, _TRUNCATE);

	return Combine(GetModRootPath("Sounds"), id);
}

bool Run(const std::string& folder, int chara)
{
	const Build* const build = BuildFor(folder);

	if (build == nullptr)
	{
		strncpy_s(g_status, "that folder holds no UNI executable", _TRUNCATE);
		return false;
	}

	const std::unique_ptr<VoiceMap::Reader> ours = VoiceMap::Open(GetModDirectory());
	const std::unique_ptr<VoiceMap::Reader> theirs = VoiceMap::Open(folder);

	const std::string tag = VoiceMap::TagOf(*ours, chara);

	if (tag.empty())
	{
		strncpy_s(g_status, "this game's own voice files could not be read", _TRUNCATE);
		return false;
	}

	InterlockedExchange(&g_progress, 10);

	std::vector<VoiceMap::Copy> copies;
	VoiceMap::Build(*ours, *theirs, chara, tag, copies);

	if (copies.empty())
	{
		sprintf_s(g_status, "%s has no voice in that copy of %s", CharaTables::Name(chara),
			build->tag);
		return false;
	}

	InterlockedExchange(&g_progress, 20);

	const std::string root = PackFolder(chara, *build);
	RemoveTree(root);

	int written = 0;
	int done = 0;

	for (const VoiceMap::Copy& copy : copies)
	{
		std::vector<uint8_t> bytes;
		++done;

		InterlockedExchange(&g_progress,
			20 + static_cast<long>((done * 70) / static_cast<int>(copies.size())));

		if (!theirs->Read(copy.sourceFolder, copy.sourceFile, bytes) || bytes.empty())
			continue;

		if (WriteWhole(Combine(root, copy.target), bytes.data(), bytes.size()))
			++written;
	}

	if (written == 0)
	{
		strncpy_s(g_status, "no voice file could be copied out of that install", _TRUNCATE);
		RemoveTree(root);
		return false;
	}

	WritePackFile(root, chara, *build);

	sprintf_s(g_status, "%d sound(s) taken from %s for %s - converting them to Ogg now", written,
		build->tag, CharaTables::Name(chara));

	LOG("VoiceImport: %s", g_status);
	return true;
}

DWORD WINAPI Worker(void* parameter)
{
	Job* const job = static_cast<Job*>(parameter);

	if (Run(job->folder, job->chara))
		InterlockedExchange(&g_finished, 1);
	else
		g_pack[0] = '\0';

	delete job;

	InterlockedExchange(&g_progress, 100);
	InterlockedExchange(&g_busy, 0);
	return 0;
}

}

VoiceImport::Source VoiceImport::Detect(const char* folder)
{
	if (folder == nullptr || folder[0] == 0)
		return Source_None;

	const std::string root = folder;

	if (BuildFor(root) == nullptr)
		return Source_None;

	return Exists(Combine(root, "d")) ? Source_UniArchive : Source_UniLoose;
}

const char* VoiceImport::SourceName(Source source)
{
	switch (source)
	{
	case Source_UniArchive:
		return "an UNDER NIGHT IN-BIRTH install";
	case Source_UniLoose:
		return "an UNDER NIGHT IN-BIRTH install with loose files";
	default:
		break;
	}

	return "no UNDER NIGHT IN-BIRTH the mod knows";
}

bool VoiceImport::IsSupported(Source source)
{
	return source != Source_None;
}

bool VoiceImport::Begin(const char* folder, int chara)
{
	if (folder == nullptr || folder[0] == 0)
		return false;

	if (chara < 0 || chara >= CharaTables::GetCharaCount())
		return false;

	if (InterlockedCompareExchange(&g_busy, 1, 0) != 0)
		return false;

	InterlockedExchange(&g_progress, 0);
	InterlockedExchange(&g_chara, chara);
	strncpy_s(g_status, "reading that install...", _TRUNCATE);

	Job* const job = new Job{ folder, chara };
	const HANDLE thread = CreateThread(nullptr, 0, &Worker, job, 0, nullptr);

	if (thread == nullptr)
	{
		delete job;
		InterlockedExchange(&g_busy, 0);
		strncpy_s(g_status, "the import could not be started", _TRUNCATE);
		return false;
	}

	CloseHandle(thread);
	return true;
}

void VoiceImport::Update()
{
	if (InterlockedCompareExchange(&g_finished, 0, 1) != 1)
		return;

	SoundPacks::Scan();

	if (g_pack[0] != '\0')
		SoundPacks::Choose(static_cast<int>(g_chara), g_pack);

	ModFiles::Rescan();
}

bool VoiceImport::IsBusy()
{
	return g_busy != 0;
}

int VoiceImport::Progress()
{
	return static_cast<int>(g_progress);
}

const char* VoiceImport::StatusText()
{
	return g_status;
}

const char* VoiceImport::PackId()
{
	return g_pack;
}
