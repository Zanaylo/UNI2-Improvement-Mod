#include "Game/PatchPacks.h"

#include "Core/ZipArchive.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BundledPack.h"
#include "Game/GamePatches.h"
#include "Game/GameState.h"
#include "Game/PatchLibrary.h"
#include "Web/Job.h"

#include <Windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace {

constexpr int kMaxEntries = 64;

struct Pack
{
	std::string id;
	std::string name;
	std::string note;
	int patches = 0;
	bool installed = false;
};

Web::Job g_install;

std::mutex g_lock;
std::vector<Pack> g_packs;
bool g_listed = false;

std::atomic<bool> g_registerWanted{ false };
std::atomic<int> g_busy{ -1 };

constexpr int kSettleFrames = 300;

int g_settle = 0;
bool g_autoDone = false;

char g_status[256] = "the built-in pack has not been added yet";

void Say(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsprintf_s(g_status, format, args);
	va_end(args);
}

std::string Combine(const std::string& folder, const std::string& name)
{
	if (folder.empty() || folder.back() == '\\')
		return folder + name;

	return folder + "\\" + name;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool EnsureFolder(const std::string& path)
{
	if (path.empty() || Exists(path))
		return true;

	const size_t slash = path.find_last_of('\\');

	if (slash != std::string::npos && !EnsureFolder(path.substr(0, slash)))
		return false;

	return CreateDirectoryA(path.c_str(), nullptr) != FALSE ||
		GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string Trim(const std::string& text)
{
	size_t from = 0;
	size_t to = text.size();

	while (from < to && (text[from] == ' ' || text[from] == '\t'))
		++from;

	while (to > from && (text[to - 1] == ' ' || text[to - 1] == '\t' || text[to - 1] == '\r'))
		--to;

	return text.substr(from, to - from);
}

class IniText
{
public:
	void Parse(const std::string& text);

	std::string Read(const std::string& section, const char* key, const char* fallback = "") const;

	int Number(const std::string& section, const char* key, int fallback = 0) const;

private:
	std::vector<std::pair<std::string, std::string>> m_values;
};

void IniText::Parse(const std::string& text)
{
	m_values.clear();

	std::string section;
	size_t at = 0;

	while (at <= text.size())
	{
		const size_t end = text.find('\n', at);
		const std::string line = Trim(text.substr(at, end == std::string::npos
			? std::string::npos : end - at));

		at = end == std::string::npos ? text.size() + 1 : end + 1;

		if (line.empty() || line[0] == ';' || line[0] == '#')
			continue;

		if (line.front() == '[' && line.back() == ']')
		{
			section = line.substr(1, line.size() - 2);
			continue;
		}

		const size_t equals = line.find('=');

		if (equals == std::string::npos)
			continue;

		m_values.emplace_back(section + "\n" + Trim(line.substr(0, equals)),
			Trim(line.substr(equals + 1)));
	}
}

std::string IniText::Read(const std::string& section, const char* key, const char* fallback) const
{
	const std::string wanted = section + "\n" + key;

	for (const auto& value : m_values)
	{
		if (_stricmp(value.first.c_str(), wanted.c_str()) == 0)
			return value.second;
	}

	return fallback;
}

int IniText::Number(const std::string& section, const char* key, int fallback) const
{
	const std::string text = Read(section, key);

	return text.empty() ? fallback : atoi(text.c_str());
}

SYSTEMTIME ParseDate(const std::string& text)
{
	SYSTEMTIME time = {};

	int year = 0;
	int month = 0;
	int day = 0;

	if (sscanf_s(text.c_str(), "%d-%d-%d", &year, &month, &day) != 3)
		return time;

	time.wYear = static_cast<WORD>(year);
	time.wMonth = static_cast<WORD>(month);
	time.wDay = static_cast<WORD>(day);
	return time;
}

std::string SectionName(int index)
{
	char text[32] = {};
	sprintf_s(text, "Patch%d", index);
	return text;
}

std::string MarkerPath(const std::string& id)
{
	return Combine(PatchLibrary::Root(), id + ".installed");
}

void ReadBundled(std::vector<Pack>& out)
{
	for (int i = 0; i < BundledPack::Count(); ++i)
	{
		const uint8_t* data = nullptr;
		size_t size = 0;

		if (!BundledPack::Get(i, data, size))
			continue;

		ZipArchive::Source source;

		if (!source.OpenMemory(data, size))
			continue;

		std::vector<uint8_t> manifest;

		if (!source.Read(source.Find("pack.ini"), manifest))
			continue;

		IniText ini;
		ini.Parse(std::string(reinterpret_cast<const char*>(manifest.data()), manifest.size()));

		Pack pack;
		pack.id = BundledPack::Id(i);
		pack.name = ini.Read("Pack", "Name", pack.id.c_str());
		pack.note = ini.Read("Pack", "Note");
		pack.patches = ini.Number("Pack", "Count");

		out.push_back(pack);
	}
}

void MarkInstalled()
{
	std::lock_guard<std::mutex> guard(g_lock);

	for (Pack& pack : g_packs)
		pack.installed = Exists(MarkerPath(pack.id));
}

void EnsureList()
{
	if (g_listed)
		return;

	std::vector<Pack> packs;
	ReadBundled(packs);

	{
		std::lock_guard<std::mutex> guard(g_lock);
		g_packs.swap(packs);
		g_listed = true;
	}

	MarkInstalled();
}

bool CopyPack(int index, Pack& out)
{
	std::lock_guard<std::mutex> guard(g_lock);

	if (index < 0 || index >= static_cast<int>(g_packs.size()))
		return false;

	out = g_packs[index];
	return true;
}

bool WriteBlob(const std::string& root, const std::string& relative,
	const std::vector<uint8_t>& data)
{
	std::string target = Combine(root, relative);

	for (char& c : target)
	{
		if (c == '/')
			c = '\\';
	}

	ZipArchive::MakeFolders(target);

	FILE* handle = nullptr;

	if (fopen_s(&handle, target.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const bool ok = data.empty() || fwrite(data.data(), 1, data.size(), handle) == data.size();
	fclose(handle);

	return ok;
}

bool ReadLinks(ZipArchive::Source& source, std::map<std::string, std::vector<std::string>>& out)
{
	std::vector<uint8_t> table;

	if (!source.Read(source.Find("files.txt"), table))
		return false;

	const std::string text(reinterpret_cast<const char*>(table.data()), table.size());

	size_t at = 0;

	while (at < text.size())
	{
		size_t end = text.find('\n', at);

		if (end == std::string::npos)
			end = text.size();

		const std::string line = Trim(text.substr(at, end - at));
		at = end + 1;

		const size_t space = line.find(' ');

		if (space == std::string::npos || space == 0 || space + 1 >= line.size())
			continue;

		const std::string path = line.substr(space + 1);

		if (path.find("..") != std::string::npos || path.find(':') != std::string::npos)
			continue;

		out[line.substr(0, space)].push_back(path);
	}

	return !out.empty();
}

bool Unpack(ZipArchive::Source& source, const std::string& root, Web::Job& job)
{
	std::map<std::string, std::vector<std::string>> links;

	if (!ReadLinks(source, links))
	{
		job.SetError("the pack has no file table, so nothing could be unpacked");
		return false;
	}

	std::vector<uint8_t> manifest;

	if (!source.Read(source.Find("pack.ini"), manifest) || !WriteBlob(root, "pack.ini", manifest))
	{
		job.SetError("the pack has no pack.ini, so nothing could be listed");
		return false;
	}

	const int total = source.Count();
	std::vector<uint8_t> data;
	int written = 0;

	for (int i = 0; i < total; ++i)
	{
		const std::string& name = source.Name(i);

		if (name.compare(0, 2, "b/") != 0)
			continue;

		const auto found = links.find(name.substr(2));

		if (found == links.end())
			continue;

		if (!source.Read(i, data))
		{
			job.SetError("one of the packed files could not be unpacked");
			return false;
		}

		for (const std::string& path : found->second)
		{
			if (!WriteBlob(root, path, data))
			{
				job.SetError("the patch folders could not be written - check the disk has room");
				return false;
			}

			++written;
		}

		if (!job.OnProgress(static_cast<uint64_t>(i + 1), static_cast<uint64_t>(total)))
		{
			job.SetError("cancelled");
			return false;
		}
	}

	LOG("PatchPacks: unpacked %d file(s) from %d blob(s)", written, total);
	return written > 0;
}

bool InstallJob(Web::Job& job)
{
	Pack pack;

	if (!CopyPack(g_busy.load(), pack))
	{
		job.SetError("that pack is not on the list any more");
		return false;
	}

	const std::string root = PatchLibrary::Root();

	if (!EnsureFolder(root))
	{
		job.SetError("the Patches folder could not be created");
		return false;
	}

	job.SetStep("reading the built-in pack");
	job.SetIndeterminate();

	const int index = BundledPack::Find(pack.id.c_str());

	const uint8_t* data = nullptr;
	size_t size = 0;

	ZipArchive::Source source;

	if (index < 0 || !BundledPack::Get(index, data, size) || !source.OpenMemory(data, size))
	{
		job.SetError("the built-in pack could not be read");
		return false;
	}

	job.SetStep("unpacking");

	if (!Unpack(source, root, job))
		return false;

	source.Close();

	job.SetStep("adding the patches");
	job.SetIndeterminate();

	g_registerWanted.store(true);
	return true;
}

struct Entry
{
	std::string folder;
	std::string name;
	std::string note;
	SYSTEMTIME released;
};

std::vector<Entry> g_pending;
std::string g_pendingPack;
int g_addedCount = 0;
int g_alreadyCount = 0;

bool BeginRegister()
{
	g_pending.clear();
	g_addedCount = 0;
	g_alreadyCount = 0;

	Pack pack;

	if (!CopyPack(g_busy.load(), pack))
		return false;

	g_pendingPack = pack.id;

	std::vector<uint8_t> manifest;

	if (!ReadWholeFile(Combine(PatchLibrary::Root(), "pack.ini"), manifest))
	{
		Say("the pack carries no pack.ini, so nothing could be listed");
		LOG("PatchPacks: %s", g_status);
		return false;
	}

	IniText ini;
	ini.Parse(std::string(reinterpret_cast<const char*>(manifest.data()), manifest.size()));

	int count = ini.Number("Pack", "Count");

	if (count > kMaxEntries)
		count = kMaxEntries;

	for (int i = 0; i < count; ++i)
	{
		const std::string section = SectionName(i);

		Entry entry;
		entry.folder = ini.Read(section, "Folder");
		entry.name = ini.Read(section, "Name");
		entry.note = ini.Read(section, "Note");
		entry.released = ParseDate(ini.Read(section, "Released"));

		if (entry.folder.empty() || entry.name.empty())
			continue;

		g_pending.push_back(entry);
	}

	return !g_pending.empty();
}

void RegisterOne(const Entry& entry)
{
	const std::string path = Combine(PatchLibrary::Root(), entry.folder);

	if (!Exists(path))
		return;

	if (PatchLibrary::IndexOfSource(path.c_str()) >= 0)
	{
		++g_alreadyCount;
		return;
	}

	char status[256] = {};

	if (!GamePatches::Import(path, entry.name, status, sizeof(status)))
	{
		LOG("PatchPacks: %s was not added - %s", entry.name.c_str(), status);
		return;
	}

	const int at = PatchLibrary::IndexOfSource(path.c_str());

	if (at >= 0)
		GamePatches::Describe(at, entry.note, entry.released);

	++g_addedCount;
}

void EndRegister()
{
	DeleteFileA(Combine(PatchLibrary::Root(), "pack.ini").c_str());

	if (g_addedCount > 0 || g_alreadyCount > 0)
	{
		FILE* marker = nullptr;

		if (fopen_s(&marker, MarkerPath(g_pendingPack).c_str(), "wb") == 0 && marker != nullptr)
		{
			fwrite(g_pendingPack.c_str(), 1, g_pendingPack.size(), marker);
			fclose(marker);
		}
	}

	Say("%d patch(es) added, %d already on the list", g_addedCount, g_alreadyCount);
	LOG("PatchPacks: %s", g_status);

	g_pending.clear();
	MarkInstalled();
}

bool Busy()
{
	return g_install.IsRunning() || g_registerWanted.load() || !g_pending.empty();
}

void StartMissing()
{
	EnsureList();

	Pack wanted;
	int index = -1;

	{
		std::lock_guard<std::mutex> guard(g_lock);

		for (size_t i = 0; i < g_packs.size(); ++i)
		{
			if (g_packs[i].installed)
				continue;

			wanted = g_packs[i];
			index = static_cast<int>(i);
			break;
		}
	}

	if (index < 0)
	{
		g_autoDone = true;
		return;
	}

	g_busy.store(index);

	LOG("PatchPacks: installing %s on startup, %d build(s)", wanted.id.c_str(), wanted.patches);

	g_install.Start("reading the built-in pack", &InstallJob);
}

}

bool PatchPacks::IsBusy()
{
	return Busy();
}

void PatchPacks::OnFrame()
{
	bool succeeded = false;

	if (g_install.TakeCompletion(succeeded) && !succeeded)
	{
		g_registerWanted.store(false);
		g_busy.store(-1);
		g_autoDone = true;
		LOG("PatchPacks: the built-in patches could not be added - %s", g_status);
	}

	if (GameState::IsInMatch())
		return;

	if (!g_autoDone && !Busy())
	{
		if (g_settle < kSettleFrames)
		{
			++g_settle;
			return;
		}

		StartMissing();
		return;
	}

	if (g_registerWanted.exchange(false) && !BeginRegister())
	{
		g_busy.store(-1);
		g_autoDone = true;
	}

	if (g_pending.empty())
		return;

	const Entry entry = g_pending.front();
	g_pending.erase(g_pending.begin());

	Say("adding %s, %d left", entry.name.c_str(), static_cast<int>(g_pending.size()));
	RegisterOne(entry);

	if (!g_pending.empty())
		return;

	EndRegister();
	g_busy.store(-1);
	g_autoDone = true;
}

const char* PatchPacks::StatusText()
{
	return g_status;
}
