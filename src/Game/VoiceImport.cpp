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
#include <vector>
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

bool IsVoiceModFolder(const std::string& root)
{
	return Exists(Combine(Combine(root, "se"), "battle_se"));
}

std::string LeafOf(const std::string& folder)
{
	std::string trimmed = folder;

	while (!trimmed.empty() && (trimmed.back() == '\\' || trimmed.back() == '/'))
		trimmed.pop_back();

	const size_t cut = trimmed.find_last_of("\\/");

	return cut == std::string::npos ? trimmed : trimmed.substr(cut + 1);
}

void GatherVoiceMod(const std::string& root, const std::string& relative, const std::string& leaf,
	std::vector<VoiceMap::Copy>& out)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(Combine(root, relative), "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.')
			continue;

		const std::string child = relative.empty() ? found.cFileName
			: Combine(relative, found.cFileName);

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			GatherVoiceMod(root, child, leaf, out);
			continue;
		}

		if (_stricmp(LeafOf(relative).c_str(), leaf.c_str()) != 0)
			continue;

		VoiceMap::Copy copy;
		copy.sourceFolder = relative;
		copy.sourceFile = found.cFileName;
		copy.target = child;

		out.push_back(copy);
	}
	while (FindNextFileA(search, &found));

	FindClose(search);
}

int CopyAll(VoiceMap::Reader& theirs, const std::vector<VoiceMap::Copy>& copies,
	const std::string& root)
{
	int written = 0;
	int done = 0;

	for (const VoiceMap::Copy& copy : copies)
	{
		std::vector<uint8_t> bytes;
		++done;

		InterlockedExchange(&g_progress,
			20 + static_cast<long>((done * 70) / static_cast<int>(copies.size())));

		if (!theirs.Read(copy.sourceFolder, copy.sourceFile, bytes) || bytes.empty())
			continue;

		if (WriteWhole(Combine(root, copy.target), bytes.data(), bytes.size()))
			++written;
	}

	return written;
}

bool ReportTaken(int written, const char* source, int chara)
{
	sprintf_s(g_status, "%d sound(s) taken from %s for %s. Converting them to Ogg now", written,
		source, CharaTables::Name(chara));

	LOG("VoiceImport: %s", g_status);
	return true;
}

bool RunVoiceMod(const std::string& folder, int chara)
{
	char leaf[16] = {};
	sprintf_s(leaf, "chr%03d", chara);

	std::vector<VoiceMap::Copy> copies;
	GatherVoiceMod(folder, "se", leaf, copies);

	if (copies.empty())
	{
		sprintf_s(g_status, "that folder has no se\\...\\%s for %s", leaf,
			CharaTables::Name(chara));
		return false;
	}

	InterlockedExchange(&g_progress, 20);

	char id[128] = {};
	sprintf_s(id, "%s - %s", CharaTables::Name(chara), LeafOf(folder).c_str());
	strncpy_s(g_pack, id, _TRUNCATE);

	const std::string root = Combine(GetModRootPath("Sounds"), id);
	RemoveTree(root);

	VoiceMap::LooseReader theirs(folder);
	const int written = CopyAll(theirs, copies, root);

	if (written == 0)
	{
		strncpy_s(g_status, "no sound could be copied out of that folder", _TRUNCATE);
		RemoveTree(root);
		return false;
	}

	char text[512] = {};
	sprintf_s(text, "[Pack]\r\nName      = %s, %s\r\nAuthor    = \r\nSource    = a voice mod\r\n"
		"Character = %d\r\n", CharaTables::Name(chara), LeafOf(folder).c_str(), chara);

	WriteWhole(Combine(root, "pack.ini"), reinterpret_cast<const uint8_t*>(text), strlen(text));

	return ReportTaken(written, LeafOf(folder).c_str(), chara);
}

bool Run(const std::string& folder, int chara)
{
	const Build* const build = BuildFor(folder);

	if (build == nullptr)
	{
		if (IsVoiceModFolder(folder))
			return RunVoiceMod(folder, chara);

		strncpy_s(g_status, "that folder holds no UNI executable and no se folder", _TRUNCATE);
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

	const int written = CopyAll(*theirs, copies, root);

	if (written == 0)
	{
		strncpy_s(g_status, "no voice file could be copied out of that install", _TRUNCATE);
		RemoveTree(root);
		return false;
	}

	WritePackFile(root, chara, *build);

	return ReportTaken(written, build->tag, chara);
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
		return IsVoiceModFolder(root) ? Source_VoiceMod : Source_None;

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
	case Source_VoiceMod:
		return "a voice mod for this game";
	default:
		break;
	}

	return "no UNI install and no voice mod the mod knows";
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
