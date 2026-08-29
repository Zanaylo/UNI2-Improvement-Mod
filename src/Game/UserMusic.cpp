#include "Game/UserMusic.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/AudioFile.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <mutex>

namespace {

constexpr int kSlotNameLimit = 31;

const char* const kReadme =
	"Your own music\r\n"
	"==============\r\n"
	"\r\n"
	"Drop MP3, OGG Vorbis or WAV files in here and the mod picks them up at boot - no tool, no\r\n"
	"config, nothing to edit. Loose files work; so does a folder per pack, and the folder name\r\n"
	"becomes the source label beside every track in Browse.\r\n"
	"\r\n"
	"  UNI2-IM/Music/menu.mp3\r\n"
	"  UNI2-IM/Music/My Playlist/battle theme.ogg\r\n"
	"\r\n"
	"The file name becomes the track title, with underscores read as spaces, and every track loops.\r\n"
	"\r\n"
	"A track loops from the start unless you give it a loop point, which is what makes a long song\r\n"
	"repeat without replaying its intro every time. Set it in Music -> Add music under Loop from,\r\n"
	"in seconds; it is kept in loops.ini beside this file.\r\n"
	"\r\n"
	"The game will open a loose file only when it is OGG Vorbis, so an MP3 or a WAV is re-encoded on\r\n"
	"the way in. The converted copy lives in Music\\.cache, is rebuilt whenever the source changes,\r\n"
	"is not exported with your soundpacks, and can be deleted at any time. An .ogg that holds Opus\r\n"
	"or FLAC rather than Vorbis is the one thing that cannot be read - re-encode it as Vorbis or\r\n"
	"MP3.\r\n"
	"\r\n"
	"The game's slot table has a 31 character name field. A longer name, a name already taken, or a\r\n"
	"name with characters that field cannot hold no longer costs you the track: the mod gives it a\r\n"
	"short name of its own and keeps the title you gave the file. Music -> Add music lists every\r\n"
	"file it found, what it did with it, and why anything was skipped.\r\n"
	"\r\n"
	"Once they are here they behave like any other track: playable from Browse, usable in a rule,\r\n"
	"pickable into a soundpack of your own, and they travel with Export soundpacks.\r\n";

std::vector<UserMusic::Entry> g_entries;
std::mutex g_lock;
volatile long g_busy = 0;
volatile long g_changed = 0;
volatile long g_version = 0;

struct Job
{
	std::string source;
	std::string target;
};

std::string Combine(const std::string& folder, const std::string& name)
{
	std::string out = folder;

	if (!out.empty() && out.back() != '\\')
		out.push_back('\\');

	return out + name;
}

std::string Stem(const std::string& fileName)
{
	const size_t dot = fileName.find_last_of('.');
	return dot == std::string::npos ? fileName : fileName.substr(0, dot);
}

std::string Extension(const std::string& fileName)
{
	const size_t dot = fileName.find_last_of('.');

	if (dot == std::string::npos)
		return std::string();

	std::string out = fileName.substr(dot + 1);

	for (char& c : out)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

	return out;
}

bool IsSupportedExtension(const std::string& extension)
{
	return extension == "ogg" || extension == "mp3" || extension == "wav";
}

bool IsPlainName(const std::string& text)
{
	if (text.empty() || static_cast<int>(text.size()) > kSlotNameLimit)
		return false;

	for (const char c : text)
	{
		if (c < 0x21 || c > 0x7e)
			return false;

		if (strchr("\\/:*?\"<>|%", c) != nullptr)
			return false;
	}

	return true;
}

std::string Alias(const std::string& pack, const std::string& fileName)
{
	uint32_t hash = 2166136261u;

	for (const char c : pack + "\\" + fileName)
	{
		hash ^= static_cast<uint8_t>(tolower(static_cast<unsigned char>(c)));
		hash *= 16777619u;
	}

	char out[16] = {};
	sprintf_s(out, "im%08x", hash);
	return out;
}

bool NameTaken(const std::string& name)
{
	for (const UserMusic::Entry& entry : g_entries)
	{
		if (_stricmp(entry.slotName.c_str(), name.c_str()) == 0)
			return true;
	}

	return false;
}

std::string PickSlotName(const std::string& pack, const std::string& fileName)
{
	const std::string stem = Stem(fileName);

	if (IsPlainName(stem) && !NameTaken(stem))
		return stem;

	return Alias(pack, fileName);
}

std::string FolderName(const std::string& pack)
{
	std::string out;

	for (const char c : pack)
	{
		if (isalnum(static_cast<unsigned char>(c)) != 0 || c == ' ' || c == '-' || c == '_')
			out.push_back(c);
	}

	while (!out.empty() && out.front() == ' ')
		out.erase(0, 1);

	while (!out.empty() && out.back() == ' ')
		out.pop_back();

	return out.empty() ? std::string("My Music") : out;
}

std::string PrettyTitle(const std::string& fileName)
{
	std::string out = Stem(fileName);

	for (char& c : out)
	{
		if (c == '_')
			c = ' ';
	}

	return out;
}

bool CacheIsCurrent(const std::string& source, const std::string& cache)
{
	WIN32_FILE_ATTRIBUTE_DATA sourceInfo = {};
	WIN32_FILE_ATTRIBUTE_DATA cacheInfo = {};

	if (!GetFileAttributesExA(cache.c_str(), GetFileExInfoStandard, &cacheInfo))
		return false;

	if (cacheInfo.nFileSizeLow == 0 && cacheInfo.nFileSizeHigh == 0)
		return false;

	if (!GetFileAttributesExA(source.c_str(), GetFileExInfoStandard, &sourceInfo))
		return false;

	return CompareFileTime(&cacheInfo.ftLastWriteTime, &sourceInfo.ftLastWriteTime) >= 0;
}

DWORD WINAPI Worker(void* parameter)
{
	std::vector<Job>* jobs = static_cast<std::vector<Job>*>(parameter);

	for (const Job& job : *jobs)
	{
		char status[128] = {};
		const bool done = AudioFile::ConvertToOgg(job.source, job.target, status, sizeof(status));

		{
			std::lock_guard<std::mutex> guard(g_lock);

			for (UserMusic::Entry& entry : g_entries)
			{
				if (entry.sourcePath != job.source)
					continue;

				entry.status = done ? UserMusic::Status_Ready : UserMusic::Status_Rejected;
				entry.note = status;
			}
		}

		InterlockedIncrement(&g_version);
		InterlockedExchange(&g_changed, 1);
	}

	delete jobs;
	InterlockedExchange(&g_busy, 0);
	return 0;
}

void StartWorker(std::vector<Job>& jobs)
{
	if (jobs.empty())
		return;

	if (InterlockedCompareExchange(&g_busy, 1, 0) != 0)
		return;

	std::vector<Job>* owned = new std::vector<Job>(jobs);
	const HANDLE thread = CreateThread(nullptr, 0, &Worker, owned, 0, nullptr);

	if (thread == nullptr)
	{
		delete owned;
		InterlockedExchange(&g_busy, 0);
		return;
	}

	CloseHandle(thread);
}

std::string LoopsIniPath()
{
	return GetModRootPath("Music\\loops.ini");
}

double ReadLoopPoint(const std::string& slotName)
{
	char text[32] = {};

	GetPrivateProfileStringA("Loops", slotName.c_str(), "0", text, sizeof(text),
		LoopsIniPath().c_str());

	const double seconds = atof(text);
	return seconds > 0.0 ? seconds : 0.0;
}

bool Claimed(const std::string& path)
{
	for (const UserMusic::Entry& entry : g_entries)
	{
		if (_stricmp(entry.playPath.c_str(), path.c_str()) == 0)
			return true;
	}

	return false;
}

std::vector<std::string> StaleCacheFiles(const std::string& cache)
{
	std::vector<std::string> stale;

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(cache, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return stale;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		const std::string path = Combine(cache, found.cFileName);

		if (!Claimed(path))
			stale.push_back(path);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return stale;
}

void PruneCache()
{
	for (const std::string& path : StaleCacheFiles(UserMusic::CacheRoot()))
	{
		if (DeleteFileA(path.c_str()) != 0)
			LOG("UserMusic: dropped the converted copy %s, nothing needs it now", path.c_str());
	}
}

void WriteReadme(const std::string& root)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, Combine(root, "README.txt").c_str(), "wb") != 0 || handle == nullptr)
		return;

	fwrite(kReadme, 1, strlen(kReadme), handle);
	fclose(handle);
}

void ClassifyConvertible(UserMusic::Entry& entry, std::vector<Job>& jobs)
{
	const std::string cache = Combine(UserMusic::CacheRoot(), entry.slotName + ".ogg");

	entry.playPath = cache;

	if (CacheIsCurrent(entry.sourcePath, cache))
	{
		entry.status = UserMusic::Status_Ready;
		entry.note = "converted to OGG Vorbis";
		return;
	}

	entry.status = UserMusic::Status_Converting;
	entry.note = "converting to OGG Vorbis";
	jobs.push_back({ entry.sourcePath, cache });
}

void Classify(UserMusic::Entry& entry, std::vector<Job>& jobs)
{
	const AudioFile::Format format = AudioFile::Identify(entry.sourcePath);

	if (AudioFile::PlaysAsIs(format))
	{
		entry.status = UserMusic::Status_Ready;
		entry.playPath = entry.sourcePath;
		entry.note = AudioFile::FormatName(format);
		return;
	}

	if (AudioFile::CanConvert(format))
	{
		ClassifyConvertible(entry, jobs);
		return;
	}

	entry.status = UserMusic::Status_Rejected;
	entry.note = AudioFile::WhyItCannotPlay(format);
}

UserMusic::Entry Describe(const std::string& folder, const std::string& pack,
	const std::string& fileName, std::vector<Job>& jobs)
{
	UserMusic::Entry entry = {};
	entry.pack = pack;
	entry.fileName = fileName;
	entry.title = PrettyTitle(fileName);
	entry.slotName = PickSlotName(pack, fileName);
	entry.sourcePath = Combine(folder, fileName);
	entry.loopPos = ReadLoopPoint(entry.slotName);

	Classify(entry, jobs);

	if (entry.status != UserMusic::Status_Rejected && entry.slotName != Stem(fileName))
	{
		entry.note += ", listed as ";
		entry.note += entry.slotName;
	}

	return entry;
}

void ScanFolder(const std::string& folder, const std::string& pack, std::vector<Job>& jobs)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(folder, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (!IsSupportedExtension(Extension(found.cFileName)))
			continue;

		g_entries.push_back(Describe(folder, pack, found.cFileName, jobs));
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

void ScanRoot(const std::string& root, std::vector<Job>& jobs)
{
	ScanFolder(root, "Music", jobs);

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(root, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		if (found.cFileName[0] == '.')
			continue;

		ScanFolder(Combine(root, found.cFileName), found.cFileName, jobs);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

}

void UserMusic::Scan()
{
	const std::string root = Root();

	if (GetFileAttributesA(root.c_str()) == INVALID_FILE_ATTRIBUTES)
		CreateDirectoryA(root.c_str(), nullptr);

	WriteReadme(root);

	std::vector<Job> jobs;

	{
		std::lock_guard<std::mutex> guard(g_lock);

		g_entries.clear();
		ScanRoot(root, jobs);
		PruneCache();
	}

	InterlockedIncrement(&g_version);

	if (jobs.empty())
		return;

	const std::string cache = CacheRoot();

	if (GetFileAttributesA(cache.c_str()) == INVALID_FILE_ATTRIBUTES)
		CreateDirectoryA(cache.c_str(), nullptr);

	LOG("UserMusic: %d file(s) waiting to be converted", static_cast<int>(jobs.size()));

	StartWorker(jobs);
}

int UserMusic::Version()
{
	return static_cast<int>(g_version);
}

std::vector<UserMusic::Entry> UserMusic::Snapshot()
{
	std::lock_guard<std::mutex> guard(g_lock);
	return g_entries;
}

bool UserMusic::ConsumeChanged()
{
	return InterlockedCompareExchange(&g_changed, 0, 1) == 1;
}

bool UserMusic::Import(const std::string& file, const std::string& pack, char* status,
	int statusSize)
{
	if (file.empty())
	{
		strncpy_s(status, statusSize, "nothing was picked", _TRUNCATE);
		return false;
	}

	const size_t slash = file.find_last_of("\\/");
	const std::string fileName = slash == std::string::npos ? file : file.substr(slash + 1);

	if (!IsSupportedExtension(Extension(fileName)))
	{
		sprintf_s(status, statusSize, "%s is not an MP3, OGG or WAV", fileName.c_str());
		return false;
	}

	const AudioFile::Format format = AudioFile::Identify(file);

	if (!AudioFile::PlaysAsIs(format) && !AudioFile::CanConvert(format))
	{
		sprintf_s(status, statusSize, "%s is %s", fileName.c_str(),
			AudioFile::WhyItCannotPlay(format));
		return false;
	}

	const std::string folder = PackFolder(FolderName(pack));

	CreateDirectoryA(Root().c_str(), nullptr);

	if (GetFileAttributesA(folder.c_str()) == INVALID_FILE_ATTRIBUTES &&
		!CreateDirectoryA(folder.c_str(), nullptr))
	{
		sprintf_s(status, statusSize, "could not make the folder %s", folder.c_str());
		return false;
	}

	const std::string target = Combine(folder, fileName);

	if (_stricmp(target.c_str(), file.c_str()) != 0 &&
		CopyFileA(file.c_str(), target.c_str(), FALSE) == 0)
	{
		sprintf_s(status, statusSize, "could not copy %s in", fileName.c_str());
		return false;
	}

	sprintf_s(status, statusSize, "added %s to %s", fileName.c_str(), FolderName(pack).c_str());
	LOG("UserMusic: %s", status);
	return true;
}

void UserMusic::SetLoopPoint(const std::string& slotName, double seconds)
{
	const double clamped = seconds > 0.0 ? seconds : 0.0;

	char text[32] = {};
	sprintf_s(text, "%.3f", clamped);

	WritePrivateProfileStringA("Loops", slotName.c_str(), text, LoopsIniPath().c_str());

	{
		std::lock_guard<std::mutex> guard(g_lock);

		for (Entry& entry : g_entries)
		{
			if (entry.slotName == slotName)
				entry.loopPos = clamped;
		}
	}

	InterlockedIncrement(&g_version);
}

std::string UserMusic::Root()
{
	return GetModRootPath("Music");
}

std::string UserMusic::CacheRoot()
{
	return GetModRootPath("Music\\.cache");
}

std::string UserMusic::PackFolder(const std::string& pack)
{
	return Combine(Root(), pack);
}

const char* UserMusic::SupportedFilter()
{
	return "Music\0*.mp3;*.ogg;*.wav\0MP3\0*.mp3\0OGG\0*.ogg\0WAV\0*.wav\0All files\0*.*\0\0";
}
