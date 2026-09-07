#include "Game/StageImport.h"

#include "Core/Settings.h"
#include "Core/TextEncoding.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgGrade.h"
#include "Game/BgListOverride.h"
#include "Game/FbGameFolder.h"
#include "Game/ModFiles.h"
#include "Game/BgObjectFix.h"
#include "Game/StageArchive.h"
#include "Game/StageThumb.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kSection = "Stages";
constexpr const char* kObjectList = "object.txt";
constexpr const char* kStageNote = "stage.txt";
constexpr const char* kCustomGame = "Custom stage";
constexpr const char* kModelLeaf = "bg.fbx.bin";
constexpr size_t kNameBytes = 62;
constexpr int kNoThumbnail = -1;
constexpr int kDfciPriorityFloor = 700;
constexpr size_t kFetchThreads = 4;

const char* const kCarried[] = {
	"Scale", "Position", "ViewGrid", "FOV", "ViewRotationX", "VanishingPoint",
	"IsFog", "FogStart", "FogEnd", "FogColor", "MSAA", "StageW", "IsBloom",
	"LightType", "LightColor", "BlanchChara", "BlanchStage",
	"ShadowLightType", "ShadowLightStatus", "ShadowReflexColor",
	"InterpolationType", "InterpolationNum",
	"BGBloomEnable", "BGBloomBlightness", "BGBloomPower", "BGBloomBiassR", "BGBloomBiassG",
	"BGBloomBiassB", "BGBloomBlurRadius", "BGBloomTextureSize", "BGBloomAlpha",
	"BGTinyFXAAEnable", "BGTinyFXAAThreshold", "BGTinyFXAALerpT",
};

struct StageNameEntry
{
	FbGameFolder::Game game;
	const char* folder;
	const char* name;
	int thumbnail;
};

#include "Game/StageNames.inc"

struct Defaulted
{
	const char* key;
	const char* value;
};

const Defaulted kDefaults[] = {
	{ "ShadowScale", "0.6" },
	{ "ShadowAlpha", "0.7" },
	{ "BGBloomEnable", "1" },
	{ "BGBloomBlightness", "0.8" },
	{ "BGBloomPower", "2.00" },
	{ "BGBloomBiassR", "1.0" },
	{ "BGBloomBiassG", "1.0" },
	{ "BGBloomBiassB", "1.0" },
	{ "BGBloomBlurRadius", "1.20" },
	{ "BGBloomTextureSize", "256" },
	{ "BGBloomAlpha", "0.5" },
	{ "BGTinyFXAAEnable", "0" },
	{ "BGTinyFXAAThreshold", "0.2" },
	{ "BGTinyFXAALerpT", "0.5" },
};

const Defaulted kCapped[] = {
	{ "BGBloomTextureSize", "256" },
};

const Defaulted kFloored[] = {
	{ "StageW", "4096" },
};

bool Floored(const char* key, std::string& value)
{
	for (const Defaulted& floored : kFloored)
	{
		if (_stricmp(floored.key, key) != 0)
			continue;

		if (atoi(value.c_str()) >= atoi(floored.value))
			return false;

		value = floored.value;
		return true;
	}

	return false;
}

bool Capped(const char* key, std::string& value)
{
	for (const Defaulted& cap : kCapped)
	{
		if (_stricmp(cap.key, key) != 0)
			continue;

		if (atoi(value.c_str()) <= atoi(cap.value))
			return false;

		value = cap.value;
		return true;
	}

	return false;
}

const Defaulted kDfciForced[] = {
	{ "IsFog", "1" },
	{ "FogStart", "0.0" },
	{ "FogEnd", "1000.0" },
	{ "FogColor", "[ 0.0, 0.0, 0.0, 0.0 ]" },
	{ "MSAA", "4" },
	{ "BGBloomEnable", "0" },
	{ "BGBloomBlightness", "0.5" },
	{ "BGBloomPower", "3.50" },
	{ "BGBloomBiassR", "0.5" },
	{ "BGBloomBiassG", "0.3" },
	{ "BGBloomBiassB", "0.5" },
	{ "BGBloomBlurRadius", "0.80" },
	{ "BGBloomAlpha", "0.25" },
};

bool Forced(FbGameFolder::Game game, const char* key)
{
	if (game != FbGameFolder::Game_DFCI)
		return false;

	for (const Defaulted& forced : kDfciForced)
	{
		if (_stricmp(forced.key, key) == 0)
			return true;
	}

	return false;
}

bool CarriesField(FbGameFolder::Game game, const char* key)
{
	return !Forced(game, key);
}

struct Job
{
	std::string folder;
	std::string stage;
	std::string name;
	std::string list;
	int number;
	bool removing;
	bool custom;
	bool fetched;
};

std::vector<StageImport::Offer> g_offers;
std::vector<StageImport::Port> g_ports;
std::string g_scanFolder;
std::string g_scanGame;

char g_status[224] = "no game looked at yet";
volatile long g_busy = 0;
volatile long g_finished = 0;
bool g_dropped[StageImport::kLastNumber + 1] = {};
volatile long g_progress = 0;

bool Numbered(int number)
{
	return number >= 0 && number <= StageImport::kLastNumber;
}

std::string BgRoot()
{
	return GetModRootPath("Mods\\bg");
}

std::string StageRoot(int number)
{
	char leaf[16] = {};
	sprintf_s(leaf, "\\bg%03d", number);

	return BgRoot() + leaf;
}

bool WriteWhole(const std::string& path, const std::vector<uint8_t>& data)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const size_t written = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), handle);
	fclose(handle);

	return written == data.size();
}

void DeleteTree(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		DeleteFileA((folder + "\\" + found.cFileName).c_str());
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	RemoveDirectoryA(folder.c_str());
}

std::string Key(int number)
{
	char key[16] = {};
	sprintf_s(key, "Stage%d", number);

	return key;
}

void LoadPorts()
{
	g_ports.clear();

	for (int number = StageImport::kFirstNumber; number <= StageImport::kLastNumber; ++number)
	{
		char stored[256] = {};

		GetPrivateProfileStringA(kSection, Key(number).c_str(), "", stored, sizeof(stored),
			Settings::GetIniPath().c_str());

		if (stored[0] == 0)
			continue;

		std::string text = stored;
		StageImport::Port port = {};
		port.number = number;

		const size_t game = text.find('|');
		const size_t folder = game == std::string::npos ? game : text.find('|', game + 1);

		if (folder == std::string::npos)
			continue;

		port.game = text.substr(0, game);
		port.folder = text.substr(game + 1, folder - game - 1);
		port.name = text.substr(folder + 1);

		g_ports.push_back(port);
	}
}

void RememberPort(const StageImport::Port& port)
{
	for (StageImport::Port& known : g_ports)
	{
		if (known.number != port.number)
			continue;

		known = port;
		return;
	}

	g_ports.push_back(port);
}

void SavePort(const StageImport::Port& port)
{
	const std::string value = port.game + "|" + port.folder + "|" + port.name;

	Settings::SaveString(kSection, Key(port.number).c_str(), value.c_str());

	RememberPort(port);
}

FbGameFolder::Game GameNamed(const std::string& name)
{
	const FbGameFolder::Game games[] = {
		FbGameFolder::Game_UNI, FbGameFolder::Game_MBTL, FbGameFolder::Game_MBAA
	};

	for (FbGameFolder::Game game : games)
	{
		if (name == FbGameFolder::Name(game))
			return game;
	}

	return FbGameFolder::Game_None;
}

const StageNameEntry* Known(FbGameFolder::Game game, const std::string& folder)
{
	for (const StageNameEntry& entry : kStageNames)
	{
		if (entry.game == game && _stricmp(entry.folder, folder.c_str()) == 0)
			return &entry;
	}

	return nullptr;
}

std::string English(FbGameFolder::Game game, const std::string& folder)
{
	const StageNameEntry* const entry = Known(game, folder);

	return entry == nullptr ? std::string() : entry->name;
}

int CustomThumbnail(const Job& job)
{
	if (!StageThumb::TakeFolder(job.folder, job.number))
		return kNoThumbnail;

	return StageThumb::CellFor(job.number);
}

int Thumbnail(const Job& job, const std::string& list, const std::string& block)
{
	const FbGameFolder::Game game = FbGameFolder::Detect(job.folder.c_str());

	int cell = -1;

	if (game == FbGameFolder::Game_DFCI)
	{
		cell = StageArchive::CardIndex(list, job.stage);
	}
	else
	{
		std::string field;

		if (StageArchive::Field(block, "StageSelTex", field))
			cell = atoi(field.c_str());
	}

	if (cell < 0 || !StageThumb::Take(game, job.folder, cell, job.number))
	{
		LOG("StageImport: stage %d gets no card - its source has none to lift", job.number);

		return kNoThumbnail;
	}

	return StageThumb::CellFor(job.number);
}

std::string EntryText(const std::string& block, int number, const std::string& shiftJisName,
	int thumbnail, FbGameFolder::Game game)
{
	char header[128] = {};
	sprintf_s(header, "\tBg_%03d =\r\n\t{\r\n\t\tName = \"", number);

	std::string out = header;
	out += shiftJisName;

	char data[64] = {};
	sprintf_s(data, "\",\r\n\t\tDataFile = \"bg%03d\",\r\n\r\n", number);
	out += data;

	for (const char* key : kCarried)
	{
		std::string value;

		if (!CarriesField(game, key) || !StageArchive::Field(block, key, value))
			continue;

		if (Capped(key, value))
			LOG("StageImport: stage %d asked for a bigger %s than the game's own stages use",
				number, key);

		if (Floored(key, value))
			LOG("StageImport: stage %d asked for a smaller %s than the game's own stages use, "
				"which moves the walls", number, key);

		out += std::string("\t\t") + key + " = " + value + ",\r\n";
	}

	for (const Defaulted& fallback : kDefaults)
	{
		if (Forced(game, fallback.key))
			continue;

		std::string value;

		if (!StageArchive::Field(block, fallback.key, value))
			out += std::string("\t\t") + fallback.key + " = " + fallback.value + ",\r\n";
	}

	if (game == FbGameFolder::Game_DFCI)
	{
		for (const Defaulted& forced : kDfciForced)
			out += std::string("\t\t") + forced.key + " = " + forced.value + ",\r\n";
	}

	char tail[64] = {};
	sprintf_s(tail, "\t\tStageSelTex = %d,\r\n\t}\r\n", thumbnail);
	out += tail;

	return out;
}

std::string ShiftJis(const std::string& utf8)
{
	std::string out;

	if (!TextEncoding::Utf8ToShiftJis(utf8, out))
		out = utf8;

	out.erase(std::remove(out.begin(), out.end(), '"'), out.end());
	out.erase(TextEncoding::ShiftJisBoundary(out, kNameBytes));

	return out;
}

void TrimTable(std::vector<uint8_t>& data)
{
	const std::string text(data.begin(), data.end());
	const size_t table = text.find("<-");
	const size_t open = table == std::string::npos ? table : text.find('{', table);
	const size_t end = open == std::string::npos
		? std::string::npos : StageArchive::MatchPair(text, open);

	if (end == std::string::npos || end >= data.size())
		return;

	data.resize(end);
}

void Rename(const std::string& stage, int number, std::vector<uint8_t>& data)
{
	const std::string was = "./bg/" + stage + "/";

	char now[24] = {};
	sprintf_s(now, "./bg/bg%03d/", number);

	std::string text(data.begin(), data.end());

	for (size_t at = text.find(was); at != std::string::npos; at = text.find(was, at))
		text.replace(at, was.size(), now);

	data.assign(text.begin(), text.end());
}

void RenameAny(int number, std::vector<uint8_t>& data)
{
	const std::string mark = "./bg/";

	char now[24] = {};
	sprintf_s(now, "./bg/bg%03d/", number);

	std::string text(data.begin(), data.end());

	for (size_t at = text.find(mark); at != std::string::npos; at = text.find(mark, at))
	{
		const size_t close = text.find('/', at + mark.size());

		if (close == std::string::npos)
			break;

		text.replace(at, close + 1 - at, now);
		at += strlen(now);
	}

	data.assign(text.begin(), text.end());
}

bool Copy(StageArchive::Source& source, const Job& job)
{
	std::vector<std::string> files;
	source.Files(job.stage, files);

	if (files.empty())
	{
		strncpy_s(g_status, "that stage holds no file the mod could read", _TRUNCATE);
		return false;
	}

	const std::string target = StageRoot(job.number);
	const FbGameFolder::Game game = FbGameFolder::Detect(job.folder.c_str());
	CreateDirectoryTree(target);

	int written = 0;
	int done = 0;

	for (const std::string& file : files)
	{
		InterlockedExchange(&g_progress,
			static_cast<long>(5 + (done++ * 85) / static_cast<int>(files.size())));

		const std::string lowered = file.size() < 4 ? file : file.substr(file.size() - 4);

		if (_stricmp(lowered.c_str(), "json") == 0 || _stricmp(lowered.c_str(), ".fbx") == 0)
			continue;

		std::vector<uint8_t> data;

		if (!source.Read(job.stage, file, data) || !StageArchive::MagicOk(file, data))
		{
			LOG("StageImport: %s\\%s came out wrong and was left out", job.stage.c_str(),
				file.c_str());
			continue;
		}

		if (_stricmp(file.c_str(), kObjectList) == 0)
		{
			const std::string leaf = BgObjectFix::SpriteFile(data);
			std::vector<uint8_t> pat;

			if (!leaf.empty())
				source.Read(job.stage, leaf, pat);

			const BgObjectFix::Report fixes = BgObjectFix::Apply(pat, data,
				game == FbGameFolder::Game_DFCI ? kDfciPriorityFloor : 0);

			if (fixes.sprites != 0 || fixes.raised != 0 || fixes.renumbered != 0)
				LOG("StageImport: %s object layer - %d sprite name(s) repaired, %d priority "
					"raised, %d block(s) renumbered", job.stage.c_str(), fixes.sprites,
					fixes.raised, fixes.renumbered);

			for (const std::string& name : fixes.lost)
				LOG("StageImport: %s draws sprite %s and %s holds no such pattern",
					kObjectList, name.c_str(), leaf.c_str());

			Rename(job.stage, job.number, data);
			TrimTable(data);
		}

		if (WriteWhole(target + "\\" + file, data))
			++written;
	}

	if (written != 0)
		return true;

	strncpy_s(g_status, "nothing in that stage could be decrypted", _TRUNCATE);
	return false;
}

bool ReadWhole(const std::string& path, std::vector<uint8_t>& data)
{
	data.clear();

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	fseek(handle, 0, SEEK_END);
	const long bytes = ftell(handle);
	fseek(handle, 0, SEEK_SET);

	if (bytes <= 0)
	{
		fclose(handle);
		return false;
	}

	data.resize(static_cast<size_t>(bytes));
	const size_t read = fread(data.data(), 1, data.size(), handle);
	fclose(handle);

	if (read == data.size())
		return true;

	data.clear();
	return false;
}

void WriteNote(const std::string& target, const Job& job, const std::string& block)
{
	const std::string from = job.custom ? std::string(kCustomGame) : job.folder;

	std::string note = "// UNI2 Improvement Mod\r\n";
	note += "Name = \"" + job.name + "\"\r\n";
	note += "From = \"" + from + "\"\r\n";
	note += "Source = \"" + job.stage + "\"\r\n\r\n";
	note += block;

	WriteWhole(target + "\\" + kStageNote, std::vector<uint8_t>(note.begin(), note.end()));
}

bool FetchFolder(Job& job)
{
	const std::string target = StageRoot(job.number);
	CreateDirectoryTree(target);

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((job.folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	int written = 0;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		std::vector<uint8_t> data;

		if (!ReadWhole(job.folder + "\\" + found.cFileName, data))
			continue;

		if (_stricmp(found.cFileName, kStageNote) == 0)
		{
			job.list.assign(data.begin(), data.end());
			continue;
		}

		if (_stricmp(found.cFileName, kObjectList) == 0)
			RenameAny(job.number, data);

		if (WriteWhole(target + "\\" + found.cFileName, data))
			++written;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	if (written == 0)
	{
		strncpy_s(g_status, "there was nothing in that folder the mod could read", _TRUNCATE);
		return false;
	}

	job.fetched = true;
	return true;
}

bool Fetch(Job& job)
{
	if (job.custom)
		return FetchFolder(job);

	StageArchive::Source* const source = StageArchive::Open(job.folder.c_str());

	if (source == nullptr)
		return false;

	const bool ok = Copy(*source, job);

	if (ok)
		source->BgList(job.list);

	delete source;

	job.fetched = ok;
	return ok;
}

bool Register(const Job& job)
{
	std::string block;

	if (job.custom)
		block = job.list;
	else
		StageArchive::Block(job.list, job.stage, block);

	const int cell = job.custom ? CustomThumbnail(job) : Thumbnail(job, job.list, block);
	const FbGameFolder::Game game = job.custom ? FbGameFolder::Game_None
		: FbGameFolder::Detect(job.folder.c_str());

	if (!BgListOverride::Add(job.number,
		EntryText(block, job.number, ShiftJis(job.name), cell, game), ShiftJis(job.name)))
	{
		DeleteTree(StageRoot(job.number));
		return false;
	}

	WriteNote(StageRoot(job.number), job, block);

	StageImport::Port port;
	port.number = job.number;
	port.game = job.custom ? kCustomGame : FbGameFolder::Name(game);
	port.folder = job.stage;
	port.name = job.name;

	SavePort(port);

	sprintf_s(g_status, "%s is stage %d now - restart the game to play it", job.name.c_str(),
		job.number);

	LOG("StageImport: %s", g_status);
	return true;
}

bool Drop(const Job& job)
{
	DeleteTree(StageRoot(job.number));
	StageThumb::Drop(job.number);

	if (!BgListOverride::Drop(job.number))
	{
		strncpy_s(g_status, "the stage's files are gone but BgList.txt could not be rewritten",
			_TRUNCATE);
		return false;
	}

	Settings::SaveString(kSection, Key(job.number).c_str(), "");
	BgGrade::Forget(job.number);

	if (Numbered(job.number))
		g_dropped[job.number] = true;

	sprintf_s(g_status, "stage %d removed - restart the game to clear it", job.number);
	return true;
}

struct Batch
{
	std::vector<Job> jobs;
	volatile long next;
};

DWORD WINAPI Fetcher(void* parameter)
{
	Batch* const batch = static_cast<Batch*>(parameter);
	const long count = static_cast<long>(batch->jobs.size());

	for (;;)
	{
		const long index = InterlockedIncrement(&batch->next) - 1;

		if (index >= count)
			break;

		Fetch(batch->jobs[index]);
		InterlockedExchange(&g_progress, 5 + (index * 80) / (count < 1 ? 1 : count));
	}

	return 0;
}

void RunBatch(Batch& batch)
{
	const size_t wanted = batch.jobs.size() < kFetchThreads ? batch.jobs.size() : kFetchThreads;
	HANDLE threads[kFetchThreads] = {};
	size_t started = 0;

	for (size_t i = 0; i < wanted; ++i)
	{
		threads[started] = CreateThread(nullptr, 0, &Fetcher, &batch, 0, nullptr);

		if (threads[started] != nullptr)
			++started;
	}

	if (started == 0)
	{
		Fetcher(&batch);
		return;
	}

	WaitForMultipleObjects(static_cast<DWORD>(started), threads, TRUE, INFINITE);

	for (size_t i = 0; i < started; ++i)
		CloseHandle(threads[i]);
}

DWORD WINAPI Worker(void* parameter)
{
	Batch* const batch = static_cast<Batch*>(parameter);

	if (batch->jobs.size() == 1 && batch->jobs.front().removing)
	{
		Drop(batch->jobs.front());
	}
	else
	{
		RunBatch(*batch);

		int done = 0;

		for (const Job& job : batch->jobs)
		{
			if (!job.fetched)
			{
				DeleteTree(StageRoot(job.number));
				continue;
			}

			done += Register(job) ? 1 : 0;
		}

		if (done == 0)
			strncpy_s(g_status, "nothing in that stage could be read", _TRUNCATE);
		else if (batch->jobs.size() > 1)
			sprintf_s(g_status, "%d of %d stage(s) installed - restart the game to play them",
				done, static_cast<int>(batch->jobs.size()));
	}

	delete batch;

	InterlockedExchange(&g_finished, 1);
	InterlockedExchange(&g_progress, 100);
	InterlockedExchange(&g_busy, 0);
	return 0;
}

bool Start(Batch* batch)
{
	if (batch->jobs.empty() || InterlockedCompareExchange(&g_busy, 1, 0) != 0)
	{
		delete batch;
		return false;
	}

	InterlockedExchange(&g_progress, 0);

	const HANDLE thread = CreateThread(nullptr, 0, &Worker, batch, 0, nullptr);

	if (thread == nullptr)
	{
		delete batch;
		InterlockedExchange(&g_busy, 0);
		strncpy_s(g_status, "the import could not be started", _TRUNCATE);
		return false;
	}

	CloseHandle(thread);
	return true;
}

}

void StageImport::Initialize()
{
	LoadPorts();

	std::vector<std::pair<int, std::string> > named;

	for (Port& port : g_ports)
	{
		const std::string english = English(GameNamed(port.game), port.folder);

		if (!english.empty() && english != port.name)
		{
			port.name = english;
			SavePort(port);
		}

		named.push_back(std::make_pair(port.number, ShiftJis(port.name)));
	}

	BgListOverride::SetNames(named);
}

bool StageImport::Scan(const char* folder)
{
	if (IsBusy())
		return false;

	g_offers.clear();
	g_scanFolder.clear();
	g_scanGame.clear();

	const FbGameFolder::Game game = FbGameFolder::Detect(folder);

	if (game != FbGameFolder::Game_MBTL && game != FbGameFolder::Game_UNI
		&& game != FbGameFolder::Game_DFCI)
	{
		sprintf_s(g_status, "that folder holds %s, and its stages are not models the mod can "
			"port", FbGameFolder::Name(game));
		return false;
	}

	StageArchive::Source* const source = StageArchive::Open(folder);

	if (source == nullptr)
	{
		strncpy_s(g_status, "that game's stage data could not be opened", _TRUNCATE);
		return false;
	}

	std::vector<StageArchive::Stage> stages;
	source->Stages(stages);
	delete source;

	for (const StageArchive::Stage& stage : stages)
	{
		Offer offer;
		offer.folder = stage.folder;
		offer.bytes = stage.bytes;
		offer.name = English(game, stage.folder);

		if (offer.name.empty())
		{
			std::string name = stage.name;

			if (!name.empty() && name.front() == '"')
				name = name.substr(1, name.size() - (name.back() == '"' ? 2 : 1));

			TextEncoding::ShiftJisToUtf8(name.c_str(), name.size(), offer.name);
		}

		g_offers.push_back(offer);
	}

	if (g_offers.empty())
	{
		strncpy_s(g_status, "no stage was found in that install", _TRUNCATE);
		return false;
	}

	g_scanFolder = folder;
	g_scanGame = FbGameFolder::Name(game);

	sprintf_s(g_status, "%d stage(s) in %s", static_cast<int>(g_offers.size()), g_scanGame.c_str());
	return true;
}

const char* StageImport::ScannedGame()
{
	return g_scanGame.c_str();
}

int StageImport::OfferCount()
{
	return static_cast<int>(g_offers.size());
}

const StageImport::Offer* StageImport::OfferAt(int index)
{
	if (index < 0 || index >= OfferCount())
		return nullptr;

	return &g_offers[index];
}

bool StageImport::Install(int index, const char* name)
{
	const int one = index;

	return InstallMany(&one, &name, 1);
}

bool StageImport::InstallMany(const int* indices, const char* const* names, int count)
{
	if (indices == nullptr || names == nullptr || count <= 0 || g_scanFolder.empty())
		return false;

	Batch* const batch = new Batch();
	batch->next = 0;

	std::vector<int> taken;

	for (int i = 0; i < count; ++i)
	{
		const Offer* const offer = OfferAt(indices[i]);

		if (offer == nullptr || names[i] == nullptr || names[i][0] == 0)
			continue;

		const int number = FreeNumber(taken);

		if (number < 0)
		{
			strncpy_s(g_status, "every stage number the mod may use is taken", _TRUNCATE);
			break;
		}

		taken.push_back(number);

		Job job;
		job.folder = g_scanFolder;
		job.stage = offer->folder;
		job.name = names[i];
		job.number = number;
		job.removing = false;
		job.custom = false;
		job.fetched = false;

		batch->jobs.push_back(job);
	}

	if (batch->jobs.empty())
	{
		delete batch;
		return false;
	}

	if (batch->jobs.size() == 1)
		sprintf_s(g_status, "installing %s as stage %d...", batch->jobs.front().name.c_str(),
			batch->jobs.front().number);
	else
		sprintf_s(g_status, "installing %d stage(s)...", static_cast<int>(batch->jobs.size()));

	return Start(batch);
}

bool StageImport::InstallFolder(const char* folder, const char* name)
{
	if (folder == nullptr || folder[0] == 0)
		return false;

	std::string leaf = folder;
	const size_t slash = leaf.find_last_of("\\/");

	if (slash != std::string::npos)
		leaf = leaf.substr(slash + 1);

	const int number = FreeNumber();

	if (number < 0)
	{
		strncpy_s(g_status, "every stage number the mod may use is taken", _TRUNCATE);
		return false;
	}

	Batch* const batch = new Batch();
	batch->next = 0;

	Job job;
	job.folder = folder;
	job.stage = leaf;
	job.name = name == nullptr || name[0] == 0 ? leaf : name;
	job.number = number;
	job.removing = false;
	job.custom = true;
	job.fetched = false;

	batch->jobs.push_back(job);

	sprintf_s(g_status, "installing %s as stage %d...", job.name.c_str(), number);

	return Start(batch);
}

bool StageImport::Remove(int number)
{
	Batch* const batch = new Batch();
	batch->next = 0;

	Job job;
	job.number = number;
	job.removing = true;
	job.custom = false;
	job.fetched = false;

	batch->jobs.push_back(job);

	sprintf_s(g_status, "removing stage %d...", number);

	return Start(batch);
}

bool StageImport::Dropped(int number)
{
	return Numbered(number) && g_dropped[number];
}

int StageImport::PortCount()
{
	return static_cast<int>(g_ports.size());
}

const StageImport::Port* StageImport::PortAt(int index)
{
	if (index < 0 || index >= PortCount())
		return nullptr;

	return &g_ports[index];
}

int StageImport::FreeNumber()
{
	static const std::vector<int> none;

	return FreeNumber(none);
}

int StageImport::FreeNumber(const std::vector<int>& reserved)
{
	for (int number = kFirstNumber; number <= kLastNumber; ++number)
	{
		const std::string folder = StageRoot(number);

		if (GetFileAttributesA(folder.c_str()) != INVALID_FILE_ATTRIBUTES)
			continue;

		bool taken = std::find(reserved.begin(), reserved.end(), number) != reserved.end();

		for (const Port& port : g_ports)
			taken = taken || port.number == number;

		if (!taken)
			return number;
	}

	return -1;
}

void StageImport::Update()
{
	if (InterlockedCompareExchange(&g_finished, 0, 1) != 1)
		return;

	LoadPorts();
	ModFiles::Rescan();
}

bool StageImport::IsBusy()
{
	return InterlockedCompareExchange(&g_busy, 0, 0) != 0;
}

int StageImport::Progress()
{
	return static_cast<int>(InterlockedCompareExchange(&g_progress, 0, 0));
}

bool StageImport::NeedsRestart()
{
	return BgListOverride::NeedsRestart();
}

const char* StageImport::StatusText()
{
	return g_status;
}
