#include "Game/Stages/StageImport.h"

#include "Core/Formats/TextEncoding.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/BgGrade.h"
#include "Game/Stages/BgListOverride.h"
#include "Game/Stages/BgMipmaps.h"
#include "Game/Stages/BgRecord.h"
#include "Game/Files/FbGameFolder.h"
#include "Game/Stages/FbxExLocal.h"
#include "Game/Files/ModFiles.h"
#include "Game/Stages/BgObjectFix.h"
#include "Game/Files/DataArchive.h"
#include "Game/Stages/Bbtag/BbtagScript.h"
#include "Game/Stages/StageArchive.h"
#include "Game/Stages/StageCards.h"
#include "Game/Stages/StageIcons.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Stages/StageNote.h"
#include "Game/Stages/StageReplacements.h"
#include "Game/Stages/StageThumb.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kObjectList = "object.txt";
constexpr const char* kStageNote = "stage.txt";
constexpr const char* kModelFile = "bg.fbx.bin";
constexpr const char* kNodeList = "nodes.txt";
constexpr const char* kImageFolder = "bg090";

struct StageImage
{
	const char* name;
	bool shared;
};

const StageImage kStageImages[] = {
	{ "stage_color.img", true },
	{ "stage_specular.img", false },
	{ "stage_bokashi_alpha.img", false },
};
constexpr const char* kCustomGame = "Custom stage";
constexpr size_t kNameBytes = 62;
constexpr int kNoThumbnail = -1;
constexpr int kDfciPriorityFloor = 700;
constexpr size_t kFetchThreads = 4;
constexpr const char* kLibraryFull = "the library is full, there is no free stage folder left";

struct Spent
{
	DWORD open;
	DWORD read;
	DWORD write;
	size_t bytes;
	BgMipmaps::Tally mips;
};

const char* const kCarried[] = {
	"Scale", "Position", "ViewGrid", "FOV", "ViewRotationX", "VanishingPoint",
	"IsFog", "FogStart", "FogEnd", "FogColor", "MSAA", "StageW", "IsBloom",
	"LightType", "LightColor", "BlanchChara", "BlanchStage",
	"ShadowLightType", "ShadowLightStatus", "ShadowReflexColor",
	"InterpolationType", "InterpolationNum",
	"BGBloomEnable", "BGBloomBlightness", "BGBloomPower", "BGBloomBiassR", "BGBloomBiassG",
	"BGBloomBiassB", "BGBloomBlurRadius", "BGBloomTextureSize", "BGBloomAlpha",
	"BGTinyFXAAEnable", "BGTinyFXAAThreshold", "BGTinyFXAALerpT",
	"TargetW", "TargetH",
};

struct StageNameEntry
{
	FbGameFolder::Game game;
	const char* folder;
	const char* name;
	int thumbnail;
};

#include "Game/Stages/StageNames.inc"

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

const Defaulted kForced[] = {
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

bool Repaired(FbGameFolder::Game game)
{
	return game == FbGameFolder::Game_DFCI;
}

bool Forced(FbGameFolder::Game game, const char* key)
{
	if (!Repaired(game))
		return false;

	for (const Defaulted& forced : kForced)
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
	std::vector<float> flow;
	std::vector<BbtagScript::Lamp> lamps;
	int id;
	bool fading;
	bool removing;
	bool custom;
	bool fetched;
	bool replacing = false;
};

std::vector<StageImport::Offer> g_offers;
std::string g_scanFolder;
std::string g_scanGame;
FbGameFolder::Game g_scanKind = FbGameFolder::Game_None;

char g_status[224] = "no game looked at yet";
volatile long g_busy = 0;
volatile long g_finished = 0;
long g_seenFiles = -1;
bool g_dropped[StageLibrary::kIdLast + 1] = {};
volatile long g_progress = 0;

bool Numbered(int id)
{
	return id >= 0 && id <= StageLibrary::kIdLast;
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

FbGameFolder::Game GameNamed(const std::string& name)
{
	const FbGameFolder::Game games[] = {
		FbGameFolder::Game_UNI, FbGameFolder::Game_UNIEL, FbGameFolder::Game_MBTL,
		FbGameFolder::Game_MBAA, FbGameFolder::Game_DFCI, FbGameFolder::Game_BBTAG,
		FbGameFolder::Game_BBCF
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

const char* Tag(FbGameFolder::Game game)
{
	if (game == FbGameFolder::Game_UNIEL)
		return " (UNIEL)";

	if (game == FbGameFolder::Game_UNI)
		return " (UNICLR)";

	if (game == FbGameFolder::Game_BBTAG)
		return " (BBTAG)";

	if (game == FbGameFolder::Game_BBCF)
		return " (BBCF)";

	return "";
}

std::string IconOf(FbGameFolder::Game game, const std::string& folder, const std::string& name)
{
	const std::string tag = Tag(game);
	const bool tagged = !tag.empty() && name.size() > tag.size() &&
		name.compare(name.size() - tag.size(), tag.size(), tag) == 0;

	const std::string shown = StageIcons::FolderFor(game,
		tagged ? name.substr(0, name.size() - tag.size()) : name, folder);

	return shown.empty() ? StageIcons::FolderFor(game, English(game, folder), folder) : shown;
}

bool TakeBundled(FbGameFolder::Game game, const std::string& folder, int id)
{
	const uint8_t* data = nullptr;
	size_t size = 0;

	return StageIcons::Bundled(game, folder, data, size) && StageThumb::TakeImage(data, size, id);
}

bool TakeIcon(FbGameFolder::Game game, const std::string& folder, const std::string& name, int id)
{
	const std::string icon = IconOf(game, folder, name);

	if (!icon.empty() && StageThumb::TakeFolder(icon, id))
		return true;

	return TakeBundled(game, folder, id);
}

void RefreshCard(const StageLibrary::Entry& entry, bool bundleChanged)
{
	const FbGameFolder::Game game = GameNamed(entry.game);
	const std::string icon = IconOf(game, entry.folder, entry.name);

	if (!icon.empty())
	{
		if (StageIcons::Newer(icon, StageThumb::CardPath(entry.id)))
			StageThumb::TakeFolder(icon, entry.id);

		return;
	}

	if (bundleChanged || !StageThumb::HasCard(entry.id))
		TakeBundled(game, entry.folder, entry.id);
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

constexpr int kTargetLeast = 1280;
constexpr int kTargetMost = 3840;

std::string Sharpened(const std::string& block)
{
	const int wide = g_settings.stageTargetWidth;
	const int high = g_settings.stageTargetHeight;

	if (wide <= kTargetLeast || high <= 0 || wide > kTargetMost)
		return std::string();

	std::string carried;

	if (StageArchive::Field(block, "TargetW", carried))
		return std::string();

	char text[96] = {};
	sprintf_s(text, "\t\tTargetW = %d,\r\n\t\tTargetH = %d,\r\n", wide, high);

	return text;
}

void Carry(int id, const char* key, std::string value, std::string& out)
{
	if (Capped(key, value))
		LOG("StageImport: stage %d asked for a bigger %s than the game's own stages use", id, key);

	if (Floored(key, value))
		LOG("StageImport: stage %d asked for a smaller %s than the game's own stages use, "
			"which moves the walls", id, key);

	out += std::string("\t\t") + key + " = " + value + ",\r\n";
}

void CarryPort(const std::string& block, int id, FbGameFolder::Game game, std::string& out)
{
	for (const char* key : kCarried)
	{
		std::string value;

		if (CarriesField(game, key) && StageArchive::Field(block, key, value))
			Carry(id, key, value, out);
	}
}

void CarryCustom(const std::string& block, int id, std::string& out)
{
	std::vector<StageArchive::Pair> pairs;
	StageNote::Values(block, pairs);

	for (const StageArchive::Pair& pair : pairs)
		Carry(id, pair.key.c_str(), pair.value, out);

	LOG("StageImport: stage %d takes %d value(s) from its stage.txt", id, static_cast<int>(pairs.size()));
}

std::string EntryText(const std::string& block, const StageLibrary::Entry& entry,
	const std::string& shiftJisName, int thumbnail, FbGameFolder::Game game)
{
	char header[128] = {};
	sprintf_s(header, "\tBg_%03d =\r\n\t{\r\n\t\tName = \"", entry.slot);

	std::string out = header;
	out += shiftJisName;

	char data[64] = {};
	sprintf_s(data, "\",\r\n\t\tDataFile = \"bg%03d\",\r\n\r\n", entry.id);
	out += data;

	if (game == FbGameFolder::Game_None)
		CarryCustom(block, entry.id, out);
	else
		CarryPort(block, entry.id, game, out);

	for (const Defaulted& fallback : kDefaults)
	{
		if (Forced(game, fallback.key))
			continue;

		std::string value;

		if (!StageArchive::Field(block, fallback.key, value))
			out += std::string("\t\t") + fallback.key + " = " + fallback.value + ",\r\n";
	}

	if (Repaired(game))
	{
		for (const Defaulted& forced : kForced)
			out += std::string("\t\t") + forced.key + " = " + forced.value + ",\r\n";
	}

	out += Sharpened(block);

	char tail[64] = {};
	sprintf_s(tail, "\t\tStageSelTex = %d,\r\n\t}\r\n", thumbnail);
	out += tail;

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

void Rename(const std::string& stage, int id, std::vector<uint8_t>& data)
{
	const std::string was = "./bg/" + stage + "/";

	char now[24] = {};
	sprintf_s(now, "./bg/bg%03d/", id);

	std::string text(data.begin(), data.end());

	for (size_t at = text.find(was); at != std::string::npos; at = text.find(was, at))
		text.replace(at, was.size(), now);

	data.assign(text.begin(), text.end());
}

void RenameAny(int id, std::vector<uint8_t>& data)
{
	const std::string mark = "./bg/";

	char now[24] = {};
	sprintf_s(now, "./bg/bg%03d/", id);

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

bool Copy(StageArchive::Source& source, const Job& job, Spent& spent)
{
	const DWORD listing = GetTickCount();

	std::vector<std::string> files;
	source.Files(job.stage, files);

	spent.read += GetTickCount() - listing;

	if (files.empty())
	{
		strncpy_s(g_status, "that stage holds no file the mod could read", _TRUNCATE);
		return false;
	}

	const std::string target = StageLibrary::FolderOf(job.id);
	const FbGameFolder::Game game = FbGameFolder::Detect(job.folder.c_str());
	CreateDirectoryTree(target);
	BgMipmaps::Unmark(target);

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
		const DWORD reading = GetTickCount();
		const bool got = source.Read(job.stage, file, data);

		spent.read += GetTickCount() - reading;

		if (!got || !StageArchive::MagicOk(file, data))
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

			Rename(job.stage, job.id, data);
			TrimTable(data);
		}

		BgMipmaps::BakeInto(file, data, spent.mips);

		const DWORD writing = GetTickCount();
		const bool put = WriteWhole(target + "\\" + file, data);

		spent.write += GetTickCount() - writing;
		spent.bytes += data.size();

		if (put)
			++written;
	}

	if (written != 0)
		return true;

	strncpy_s(g_status, "nothing in that stage could be decrypted", _TRUNCATE);
	return false;
}

int SourceNumber(const std::string& stage)
{
	size_t at = stage.size();

	while (at > 0 && isdigit(static_cast<unsigned char>(stage[at - 1])) != 0)
		--at;

	return at == stage.size() ? -1 : atoi(stage.c_str() + at);
}

std::string ImageDonor(FbGameFolder::Game game, const std::string& stage)
{
	if (game != FbGameFolder::Game_UNI && game != FbGameFolder::Game_UNIEL)
		return std::string();

	const int number = SourceNumber(stage);

	if (number < 0)
		return std::string();

	char folder[16] = {};
	sprintf_s(folder, "bg%03d", number);

	return folder;
}

void LiftImages(const std::string& target, const std::string& donor)
{
	for (const StageImage& image : kStageImages)
	{
		std::vector<uint8_t> stub;
		DataArchive::Read(kImageFolder, image.name, stub);

		const std::string path = target + "\\" + image.name;
		std::vector<uint8_t> have;
		const bool held = ReadWhole(path, have);

		std::vector<uint8_t> wanted;

		if (!donor.empty())
			DataArchive::Read(donor.c_str(), image.name, wanted);

		if (wanted.empty() && image.shared)
			wanted = stub;

		if (wanted.empty())
		{
			if (held && have == stub && DeleteFileA(path.c_str()) != 0)
				LOG("StageImport: %s was the one-pixel %s stub, and a stage renders better "
					"without it than with it", image.name, kImageFolder);

			continue;
		}

		if (held && (have == wanted || have != stub))
			continue;

		if (WriteWhole(path, wanted))
			LOG("StageImport: %s comes from the game's own %s", image.name,
				donor.empty() ? kImageFolder : donor.c_str());
	}
}

void WriteNote(const std::string& target, const Job& job, const std::string& block)
{
	const std::string from = job.custom ? std::string(kCustomGame) : job.folder;

	std::string note = "// UNI2 Improvement Mod\r\n";
	note += "Name = \"" + job.name + "\"\r\n";
	note += "From = \"" + from + "\"\r\n";
	note += "Source = \"" + job.stage + "\"\r\n";

	if (!job.flow.empty())
	{
		note += "Flow = [ ";

		for (size_t i = 0; i < job.flow.size(); ++i)
		{
			char rate[32] = {};
			sprintf_s(rate, "%s%.8f", i == 0 ? "" : ", ", job.flow[i]);
			note += rate;
		}

		note += " ]\r\n";
	}

	for (size_t i = 0; i < job.lamps.size(); ++i)
	{
		const BbtagScript::Lamp& lamp = job.lamps[i];

		if (lamp.loop < 1 || lamp.ramp.empty())
			continue;

		char head[48] = {};
		sprintf_s(head, "Lamp%d = [ %d", static_cast<int>(i), lamp.loop);
		note += head;

		for (const BbtagScript::Ramp& ramp : lamp.ramp)
		{
			char step[64] = {};
			sprintf_s(step, ", %d, %d, %d", ramp.at, ramp.target, ramp.frames);
			note += step;
		}

		note += " ]\r\n";
	}

	if (job.fading)
		note += "VertexAlpha = 1\r\n";

	note += "\r\n";
	note += block;

	WriteWhole(target + "\\" + kStageNote, std::vector<uint8_t>(note.begin(), note.end()));
}

bool Editing(const std::string& leaf)
{
	if (_stricmp(leaf.c_str(), kNodeList) == 0)
		return true;

	const size_t dot = leaf.rfind('.');

	if (dot == std::string::npos)
		return false;

	const std::string tail = leaf.substr(dot);

	return _stricmp(tail.c_str(), ".fbx") == 0 || _stricmp(tail.c_str(), ".json") == 0;
}

bool FetchFolder(Job& job)
{
	const std::string target = StageLibrary::FolderOf(job.id);
	CreateDirectoryTree(target);
	BgMipmaps::Unmark(target);

	BgMipmaps::Tally mips = {};
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((job.folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	int written = 0;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (Editing(found.cFileName))
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
			RenameAny(job.id, data);

		BgMipmaps::BakeInto(found.cFileName, data, mips);

		if (WriteWhole(target + "\\" + found.cFileName, data))
			++written;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	if (mips.files != 0)
		LOG("BgMipmaps: %s baked %d file(s) in %lu ms at install", job.stage.c_str(), mips.files,
			static_cast<unsigned long>(mips.milliseconds));

	if (written == 0)
	{
		strncpy_s(g_status, "there was nothing in that folder the mod could read", _TRUNCATE);
		return false;
	}

	job.fetched = true;
	return true;
}

void NameFromNote(Job& job)
{
	std::string name;

	if (job.name != job.stage || !StageArchive::Field(job.list, "Name", name))
		return;

	const std::string unquoted = StageArchive::Unquoted(name);

	if (!unquoted.empty())
		job.name = unquoted;
}

bool Fetch(Job& job)
{
	const DWORD began = GetTickCount();

	if (job.custom)
	{
		if (job.replacing)
			DeleteTree(StageLibrary::FolderOf(job.id));

		if (!FetchFolder(job))
			return false;

		NameFromNote(job);

		if (!job.replacing)
			LiftImages(StageLibrary::FolderOf(job.id), std::string());

		LOG("StageImport: %s took %lu ms from a folder", job.stage.c_str(),
			static_cast<unsigned long>(GetTickCount() - began));

		return true;
	}

	Spent spent = {};
	const DWORD opening = GetTickCount();
	StageArchive::Source* const source = StageArchive::Open(job.folder.c_str());

	spent.open = GetTickCount() - opening;

	if (source == nullptr)
		return false;

	const bool ok = Copy(*source, job, spent);

	if (ok)
	{
		source->BgList(job.list);
		source->Flow(job.stage, job.flow);
		source->Lamps(job.stage, job.lamps);
		job.fading = source->Fading(job.stage);
		LiftImages(StageLibrary::FolderOf(job.id),
			ImageDonor(FbGameFolder::Detect(job.folder.c_str()), job.stage));
	}

	delete source;

	LOG("StageImport: %s took %lu ms - %lu opening the source, %lu converting and reading, %lu "
		"writing %.1f MB", job.stage.c_str(),
		static_cast<unsigned long>(GetTickCount() - began),
		static_cast<unsigned long>(spent.open), static_cast<unsigned long>(spent.read),
		static_cast<unsigned long>(spent.write),
		static_cast<double>(spent.bytes) / (1024.0 * 1024.0));

	if (spent.mips.files != 0)
		LOG("BgMipmaps: %s baked %d file(s) in %lu ms at install", job.stage.c_str(),
			spent.mips.files, static_cast<unsigned long>(spent.mips.milliseconds));

	job.fetched = ok;
	return ok;
}

void MakeCard(const Job& job, const std::string& block)
{
	if (job.custom)
	{
		StageThumb::TakeFolder(job.folder, job.id);
		return;
	}

	const FbGameFolder::Game game = FbGameFolder::Detect(job.folder.c_str());

	if (TakeIcon(game, job.stage, job.name, job.id))
		return;

	int cell = -1;

	if (game == FbGameFolder::Game_DFCI)
	{
		cell = StageArchive::CardIndex(job.list, job.stage);
	}
	else
	{
		std::string field;

		if (StageArchive::Field(block, "StageSelTex", field))
			cell = atoi(field.c_str());
	}

	if (cell >= 0 && StageThumb::Take(game, job.folder, cell, job.id))
		return;

	LOG("StageImport: stage %d gets no card - its source has none to lift", job.id);
}

bool Register(const Job& job)
{
	if (job.replacing)
	{
		WriteNote(StageLibrary::FolderOf(job.id), job, job.list);
		sprintf_s(g_status, "%s is in place of stage %d", job.name.c_str(), job.id);

		LOG("StageImport: %s", g_status);
		return true;
	}

	std::string block;

	if (job.custom)
		block = job.list;
	else
		StageArchive::Block(job.list, job.stage, block);

	const FbGameFolder::Game game = job.custom ? FbGameFolder::Game_None
		: FbGameFolder::Detect(job.folder.c_str());

	MakeCard(job, block);
	WriteNote(StageLibrary::FolderOf(job.id), job, block);

	StageLibrary::Entry entry = {};
	entry.id = job.id;
	entry.slot = -1;
	entry.shown = true;
	entry.game = job.custom ? kCustomGame : FbGameFolder::Name(game);
	entry.folder = job.stage;
	entry.name = job.name;

	StageLibrary::Put(entry);

	if (Numbered(job.id))
		g_dropped[job.id] = false;

	sprintf_s(g_status, "%s is installed", job.name.c_str());

	LOG("StageImport: %s", g_status);
	return true;
}

bool Drop(const Job& job)
{
	DeleteTree(StageLibrary::FolderOf(job.id));

	if (job.replacing)
	{
		BgListOverride::Restore(job.id);
		StageReplacements::Forget(job.id);
		sprintf_s(g_status, "stage %d is the game's own again", job.id);

		LOG("StageImport: %s", g_status);
		return true;
	}

	StageThumb::Forget(job.id);
	StageLibrary::Erase(job.id);
	BgGrade::Forget(job.id);

	if (Numbered(job.id))
		g_dropped[job.id] = true;

	sprintf_s(g_status, "%s removed", job.name.c_str());
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
				DeleteTree(StageLibrary::FolderOf(job.id));
				continue;
			}

			done += Register(job) ? 1 : 0;
		}

		if (done == 0)
			strncpy_s(g_status, "nothing in that stage could be read", _TRUNCATE);
		else if (batch->jobs.size() > 1)
			sprintf_s(g_status, "%d of %d stage(s) installed", done,
				static_cast<int>(batch->jobs.size()));
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

bool ReadNote(int id, std::string& out)
{
	std::vector<uint8_t> blob;

	if (!ReadWhole(StageLibrary::NoteOf(id), blob) || blob.empty())
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
}

void KeepNote(const StageLibrary::Entry& entry)
{
	std::string existing;

	if (ReadNote(entry.id, existing))
		return;

	std::string block;

	if (!BgListOverride::Body(entry.id, block))
		return;

	Job job = {};
	job.name = entry.name;
	job.stage = entry.folder;
	job.folder = entry.game;
	job.custom = entry.game == kCustomGame;

	WriteNote(StageLibrary::FolderOf(entry.id), job, block);

	LOG("StageImport: stage %d had no stage.txt, so its BgList block was kept in one",
		entry.id);
}

void Relocalise(const StageLibrary::Entry& entry)
{
	if (GameNamed(entry.game) != FbGameFolder::Game_UNIEL)
		return;

	const std::string path = StageLibrary::FolderOf(entry.id) + "\\" + kModelFile;

	std::vector<uint8_t> data;

	if (!ReadWhole(path, data))
		return;

	FbxExLocal::Report report = {};

	if (!FbxExLocal::Apply(data, report) || report.nodes == 0 || !WriteWhole(path, data))
		return;

	LOG("StageImport: stage %d was ported with its node matrices baked into world space - "
		"%d node(s) and %d frame(s) put back onto their parents", entry.id, report.nodes,
		report.frames);
}

bool PointRecord(const StageLibrary::Entry& entry)
{
	if (entry.slot < 0)
		return false;

	char folder[16] = {};
	sprintf_s(folder, "bg%03d", entry.id);

	std::string now;
	const bool moved = !BgRecord::FolderOf(entry.slot, now) || now != folder;

	std::string note;
	ReadNote(entry.id, note);

	const int cell = StageThumb::HasCard(entry.id)
		? StageThumb::CellFor(entry.slot) : kNoThumbnail;
	const std::string name = ShiftJis(entry.name);
	const std::string text = EntryText(note, entry, name, cell, GameNamed(entry.game));

	return BgRecord::Apply(entry.slot, entry.id, text, name, cell) && moved;
}

void Repoint()
{
	if (!BgRecord::Reachable())
		return;

	std::vector<StageLibrary::Entry> entries;
	StageLibrary::Snapshot(entries);

	int moved = 0;

	for (const StageLibrary::Entry& entry : entries)
		moved += PointRecord(entry) ? 1 : 0;

	if (moved != 0)
		StageCards::Repaint();
}

void RestoreStale()
{
	std::vector<int> stale;
	StageReplacements::Stale(stale);

	for (int number : stale)
	{
		BgListOverride::Restore(number);
		StageReplacements::Forget(number);

		LOG("StageImport: stage %d lost its stage.txt, so it is the game's own again", number);
	}
}

std::vector<BgListOverride::Reworked> Reworks()
{
	std::vector<StageReplacements::Replacement> replaced;
	StageReplacements::Snapshot(replaced);

	std::vector<BgListOverride::Reworked> reworked;

	for (const StageReplacements::Replacement& replacement : replaced)
		reworked.push_back({ replacement.number, replacement.note, ShiftJis(replacement.name) });

	return reworked;
}

void SyncList()
{
	std::vector<StageLibrary::Entry> entries;
	StageLibrary::Snapshot(entries);

	const int budget = StageLibrary::SlotBudget();

	std::vector<BgListOverride::Slotted> ours;
	std::vector<int> owned;

	for (int index = 0; index < budget; ++index)
		owned.push_back(StageLibrary::SlotAt(index));

	for (const StageLibrary::Entry& known : entries)
	{
		if (known.position < 0 || known.position >= budget)
			continue;

		StageLibrary::Entry entry = known;
		entry.slot = StageLibrary::SlotAt(known.position);

		std::string note;
		ReadNote(entry.id, note);

		const int cell = StageThumb::HasCard(entry.id)
			? StageThumb::CellFor(entry.slot) : kNoThumbnail;

		BgListOverride::Slotted one;
		one.number = entry.slot;
		one.shiftJisName = ShiftJis(entry.name);
		one.entry = EntryText(note, entry, one.shiftJisName, cell, GameNamed(entry.game));

		ours.push_back(one);
	}

	RestoreStale();
	BgListOverride::Sync(ours, owned, Reworks());
}

void Apply()
{
	SyncList();
	Repoint();
}

void Rehome()
{
	std::vector<StageLibrary::Entry> entries;
	StageLibrary::Snapshot(entries);

	for (const StageLibrary::Entry& entry : entries)
	{
		if (!StageLibrary::GameOwns(entry.id))
			continue;

		const int id = StageLibrary::FreeId(std::vector<int>());

		if (id < 0 || !MoveFileA(StageLibrary::FolderOf(entry.id).c_str(),
			StageLibrary::FolderOf(id).c_str()))
		{
			continue;
		}

		std::vector<uint8_t> objects;
		const std::string list = StageLibrary::FolderOf(id) + "\\" + kObjectList;

		if (ReadWhole(list, objects))
		{
			RenameAny(id, objects);
			WriteWhole(list, objects);
		}

		StageLibrary::Entry moved = entry;
		moved.id = id;
		moved.slot = -1;

		StageLibrary::Erase(entry.id);
		BgListOverride::Restore(entry.id);
		StageLibrary::Put(moved);

		LOG("StageImport: '%s' sat on bg%03d, which is the game's own, and is bg%03d now",
			moved.name.c_str(), entry.id, id);
	}
}

DWORD WINAPI Backfill(void*)
{
	std::vector<StageLibrary::Entry> entries;
	StageLibrary::Snapshot(entries);

	const DWORD began = GetTickCount();
	int baked = 0;

	for (const StageLibrary::Entry& entry : entries)
		baked += BgMipmaps::BakeFolder(StageLibrary::FolderOf(entry.id));

	if (baked != 0)
		LOG("BgMipmaps: %d file(s) across the installed stages baked in %lu ms, once", baked,
			static_cast<unsigned long>(GetTickCount() - began));

	InterlockedExchange(&g_busy, 0);
	return 0;
}

void StartBackfill()
{
	if (!BgMipmaps::Enabled() || InterlockedCompareExchange(&g_busy, 1, 0) != 0)
		return;

	const HANDLE thread = CreateThread(nullptr, 0, &Backfill, nullptr, CREATE_SUSPENDED, nullptr);

	if (thread == nullptr)
	{
		InterlockedExchange(&g_busy, 0);
		return;
	}

	SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
	ResumeThread(thread);
	CloseHandle(thread);
}

}

void StageImport::Initialize()
{
	StageLibrary::Load();
	Rehome();

	std::vector<StageLibrary::Entry> entries;
	StageLibrary::Snapshot(entries);

	const bool bundleChanged = StageIcons::BundleChanged();

	for (StageLibrary::Entry& entry : entries)
	{
		KeepNote(entry);
		Relocalise(entry);
		LiftImages(StageLibrary::FolderOf(entry.id),
			ImageDonor(GameNamed(entry.game), entry.folder));

		RefreshCard(entry, bundleChanged);

		const std::string english = English(GameNamed(entry.game), entry.folder);

		if (english.empty() || english == entry.name)
			continue;

		entry.name = english;
		StageLibrary::Put(entry);
	}

	if (bundleChanged)
		StageIcons::RememberBundle();

	StageReplacements::Load();
	SyncList();
	StartBackfill();

	g_seenFiles = ModFiles::Revision();
}

bool StageImport::Scan(const char* folder)
{
	if (IsBusy())
		return false;

	g_offers.clear();
	g_scanFolder.clear();
	g_scanGame.clear();
	g_scanKind = FbGameFolder::Game_None;

	const FbGameFolder::Game game = FbGameFolder::Detect(folder);

	if (game != FbGameFolder::Game_MBTL && game != FbGameFolder::Game_UNI
		&& game != FbGameFolder::Game_UNIEL && game != FbGameFolder::Game_DFCI
		&& game != FbGameFolder::Game_BBTAG && game != FbGameFolder::Game_BBCF)
	{
		sprintf_s(g_status, "that folder holds %s, and the mod cannot port its stages",
			FbGameFolder::Name(game));
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

		offer.name += Tag(game);

		g_offers.push_back(offer);
	}

	if (g_offers.empty())
	{
		strncpy_s(g_status, "no stage was found in that install", _TRUNCATE);
		return false;
	}

	g_scanFolder = folder;
	g_scanGame = FbGameFolder::Name(game);
	g_scanKind = game;

	sprintf_s(g_status, "%d stage(s) in %s", static_cast<int>(g_offers.size()), g_scanGame.c_str());
	return true;
}

const char* StageImport::ScannedGame()
{
	return g_scanGame.c_str();
}

FbGameFolder::Game StageImport::ScannedKind()
{
	return g_scanKind;
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

	std::vector<int> ids;

	for (int i = 0; i < count; ++i)
	{
		const Offer* const offer = OfferAt(indices[i]);

		if (offer == nullptr || names[i] == nullptr || names[i][0] == 0)
			continue;

		const int id = StageLibrary::FreeId(ids);

		if (id < 0)
		{
			strncpy_s(g_status, kLibraryFull, _TRUNCATE);
			break;
		}

		ids.push_back(id);

		Job job;
		job.folder = g_scanFolder;
		job.stage = offer->folder;
		job.name = names[i];
		job.id = id;
		job.removing = false;
		job.custom = false;
		job.fetched = false;
		job.fading = false;

		batch->jobs.push_back(job);
	}

	if (batch->jobs.empty())
	{
		delete batch;
		return false;
	}

	if (batch->jobs.size() == 1)
		sprintf_s(g_status, "installing %s...", batch->jobs.front().name.c_str());
	else
		sprintf_s(g_status, "installing %d stage(s)...", static_cast<int>(batch->jobs.size()));

	return Start(batch);
}

namespace {

std::string LeafOf(const char* folder)
{
	const std::string path = folder;
	const size_t slash = path.find_last_of("\\/");

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

Job FolderJob(const char* folder, int id, const char* name)
{
	Job job;
	job.folder = folder;
	job.stage = LeafOf(folder);
	job.name = name == nullptr || name[0] == 0 ? job.stage : name;
	job.id = id;
	job.fading = false;
	job.removing = false;
	job.custom = true;
	job.fetched = false;

	return job;
}

Job RemovalJob(int id, const std::string& name)
{
	Job job;
	job.id = id;
	job.name = name;
	job.fading = false;
	job.removing = true;
	job.custom = false;
	job.fetched = false;

	return job;
}

bool StartOne(const Job& job)
{
	Batch* const batch = new Batch();
	batch->next = 0;
	batch->jobs.push_back(job);

	return Start(batch);
}

}

bool StageImport::InstallFolder(const char* folder, const char* name)
{
	if (folder == nullptr || folder[0] == 0)
		return false;

	const int id = StageLibrary::FreeId(std::vector<int>());

	if (id < 0)
	{
		strncpy_s(g_status, kLibraryFull, _TRUNCATE);
		return false;
	}

	const Job job = FolderJob(folder, id, name);
	sprintf_s(g_status, "installing %s...", job.name.c_str());

	return StartOne(job);
}

bool StageImport::Remove(int id)
{
	StageLibrary::Entry entry = {};

	if (!StageLibrary::Of(id, entry))
		return false;

	sprintf_s(g_status, "removing %s...", entry.name.c_str());

	return StartOne(RemovalJob(entry.id, entry.name));
}

bool StageImport::ReplaceFolder(const char* folder, int number)
{
	if (folder == nullptr || folder[0] == 0 || number <= 0 || !StageLibrary::GameOwns(number))
		return false;

	if (_stricmp(folder, StageLibrary::FolderOf(number).c_str()) == 0)
	{
		strncpy_s(g_status, "that folder is the replacement itself. Pick the folder it came from",
			_TRUNCATE);
		return false;
	}

	Job job = FolderJob(folder, number, nullptr);
	job.replacing = true;

	sprintf_s(g_status, "putting %s in place of stage %d...", job.name.c_str(), number);

	return StartOne(job);
}

bool StageImport::Restore(int number)
{
	if (number <= 0 || !StageLibrary::GameOwns(number))
		return false;

	Job job = RemovalJob(number, std::string());
	job.replacing = true;

	sprintf_s(g_status, "restoring stage %d...", number);

	return StartOne(job);
}

bool StageImport::SetInGame(int id, bool inGame)
{
	if (IsBusy())
		return false;

	StageLibrary::Entry entry = {};

	if (!StageLibrary::Of(id, entry) || entry.shown == inGame)
		return false;

	if (inGame && StageLibrary::Room() == 0)
	{
		sprintf_s(g_status, "the picker holds %d stage(s) and all of them are in use. Take one out "
			"first", StageLibrary::SlotBudget());

		return false;
	}

	StageLibrary::Show(id, inGame);
	Apply();

	sprintf_s(g_status, "%s %s the picker", entry.name.c_str(), inGame ? "joins" : "leaves");

	LOG("StageImport: %s", g_status);
	return true;
}

bool StageImport::Dropped(int id)
{
	return Numbered(id) && g_dropped[id];
}

void StageImport::Update()
{
	if (InterlockedCompareExchange(&g_finished, 0, 1) == 1)
	{
		StageLibrary::Load();
		StageReplacements::Load();
		ModFiles::Rescan();
		Apply();
		g_seenFiles = ModFiles::Revision();
		return;
	}

	const long files = ModFiles::Revision();

	if (files == g_seenFiles || IsBusy())
		return;

	g_seenFiles = files;

	StageLibrary::Load();
	StageReplacements::Load();
	Apply();

	LOG("StageImport: the Mods folder changed, the stages were read again");
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
