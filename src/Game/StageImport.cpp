#include "Game/StageImport.h"

#include "Core/Settings.h"
#include "Core/TextEncoding.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgListOverride.h"
#include "Game/FbGameFolder.h"
#include "Game/ModFiles.h"
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
constexpr size_t kNameBytes = 62;
constexpr int kBorrowedThumbnail = 9;

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
};

struct Job
{
	std::string folder;
	std::string stage;
	std::string name;
	int number;
	bool removing;
};

std::vector<StageImport::Offer> g_offers;
std::vector<StageImport::Port> g_ports;
std::string g_scanFolder;
std::string g_scanGame;

char g_status[224] = "no game looked at yet";
volatile long g_busy = 0;
volatile long g_finished = 0;
volatile long g_progress = 0;

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

void SavePort(const StageImport::Port& port)
{
	const std::string value = port.game + "|" + port.folder + "|" + port.name;

	Settings::SaveString(kSection, Key(port.number).c_str(), value.c_str());
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

int Thumbnail(const Job& job, const std::string& block)
{
	const FbGameFolder::Game game = FbGameFolder::Detect(job.folder.c_str());
	const StageNameEntry* const known = Known(game, job.stage);

	if (known != nullptr && known->thumbnail >= 0)
		return known->thumbnail;

	std::string cell;

	if (!StageArchive::Field(block, "StageSelTex", cell))
		return kBorrowedThumbnail;

	if (!StageThumb::Take(game, job.folder, atoi(cell.c_str()), job.number))
		return kBorrowedThumbnail;

	return job.number;
}

std::string EntryText(const std::string& block, int number, const std::string& shiftJisName,
	int thumbnail)
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

		if (StageArchive::Field(block, key, value))
			out += std::string("\t\t") + key + " = " + value + ",\r\n";
	}

	for (const Defaulted& fallback : kDefaults)
	{
		std::string value;

		if (!StageArchive::Field(block, fallback.key, value))
			out += std::string("\t\t") + fallback.key + " = " + fallback.value + ",\r\n";
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

bool Add(const Job& job)
{
	StageArchive::Source* const source = StageArchive::Open(job.folder.c_str());

	if (source == nullptr)
	{
		strncpy_s(g_status, "that game's stage data could not be opened", _TRUNCATE);
		return false;
	}

	bool ok = Copy(*source, job);

	if (ok)
	{
		std::string list;
		std::string block;

		source->BgList(list);
		StageArchive::Block(list, job.stage, block);

		const int cell = Thumbnail(job, block);

		ok = BgListOverride::Add(job.number,
			EntryText(block, job.number, ShiftJis(job.name), cell), ShiftJis(job.name));
	}

	delete source;

	if (!ok)
	{
		DeleteTree(StageRoot(job.number));
		return false;
	}

	StageImport::Port port;
	port.number = job.number;
	port.game = FbGameFolder::Name(FbGameFolder::Detect(job.folder.c_str()));
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

	sprintf_s(g_status, "stage %d removed - restart the game to clear it", job.number);
	return true;
}

DWORD WINAPI Worker(void* parameter)
{
	Job* const job = static_cast<Job*>(parameter);

	job->removing ? Drop(*job) : Add(*job);

	delete job;

	InterlockedExchange(&g_finished, 1);
	InterlockedExchange(&g_progress, 100);
	InterlockedExchange(&g_busy, 0);
	return 0;
}

bool Start(Job* job)
{
	if (InterlockedCompareExchange(&g_busy, 1, 0) != 0)
	{
		delete job;
		return false;
	}

	InterlockedExchange(&g_progress, 0);

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

	if (game != FbGameFolder::Game_MBTL && game != FbGameFolder::Game_UNI)
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
	const Offer* const offer = OfferAt(index);

	if (offer == nullptr || g_scanFolder.empty() || name == nullptr || name[0] == 0)
		return false;

	const int number = FreeNumber();

	if (number < 0)
	{
		strncpy_s(g_status, "every stage number the mod may use is taken", _TRUNCATE);
		return false;
	}

	Job* const job = new Job();
	job->folder = g_scanFolder;
	job->stage = offer->folder;
	job->name = name;
	job->number = number;
	job->removing = false;

	sprintf_s(g_status, "installing %s as stage %d...", name, number);

	return Start(job);
}

bool StageImport::Remove(int number)
{
	Job* const job = new Job();
	job->number = number;
	job->removing = true;

	sprintf_s(g_status, "removing stage %d...", number);

	return Start(job);
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
	for (int number = kFirstNumber; number <= kLastNumber; ++number)
	{
		const std::string folder = StageRoot(number);

		if (GetFileAttributesA(folder.c_str()) != INVALID_FILE_ATTRIBUTES)
			continue;

		bool taken = false;

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
