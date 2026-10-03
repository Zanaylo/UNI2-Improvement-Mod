#include "Game/Stages/StageExport.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/Bbtag/BbtagExport.h"
#include "Game/Stages/StageImport.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Stages/StagePlacement.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

constexpr const char* kExportRoot = "Export";
constexpr const char* kModelFile = "bg.fbx.bin";
constexpr const char* kStagePrefix = "bg_";
constexpr const char* kModelSuffix = ".MUA";
constexpr const char* kWritten[] = { "*.MUA", "*.mmot", "*.evb", "*.dds", "*.png", "*.tga", "*.bmp", "*.pac" };
constexpr size_t kLongestStem = 40;

struct GameFolder
{
	FbGameFolder::Game game;
	const char* folder;
};

constexpr GameFolder kGameFolders[] = { { FbGameFolder::Game_BBTAG, "BBTAG" }, { FbGameFolder::Game_BBCF, "BBCF" } };

struct Job
{
	int id;
	std::string name;
	std::string from;
	std::string folder;
	std::string stage;
	BbtagExport::Framing framing;
	FbGameFolder::Game game;
	StageInstall::Target target;
};

volatile long g_busy = 0;
std::mutex g_lock;
std::string g_status = "Nothing exported yet.";
std::string g_folder;

void Report(const std::string& text, const std::string& folder = std::string())
{
	std::lock_guard<std::mutex> hold(g_lock);
	g_status = text;

	if (!folder.empty())
		g_folder = folder;
}

std::string Stem(const std::string& name, int id)
{
	std::string out = kStagePrefix;
	bool gap = false;

	for (char letter : name)
	{
		const unsigned char code = static_cast<unsigned char>(letter);

		if (code < 0x80 && isalnum(code))
		{
			if (gap && out.size() > strlen(kStagePrefix))
				out.push_back('_');

			out.push_back(static_cast<char>(tolower(code)));
			gap = false;
			continue;
		}

		gap = true;
	}

	if (out.size() == strlen(kStagePrefix))
		out += "uni2_" + std::to_string(id);

	return out.substr(0, kLongestStem);
}

bool Triple(const std::string& text, float out[3])
{
	return sscanf_s(text.c_str(), " [ %f , %f , %f", &out[0], &out[1], &out[2]) == 3;
}

bool Single(const std::string& text, float& out)
{
	return sscanf_s(text.c_str(), " %f", &out) == 1;
}

BbtagExport::Framing FramingOf(int id, int slot)
{
	BbtagExport::Framing out = BbtagExport::Neutral();
	StagePlacement::Place place = {};

	if (slot >= 0 && StagePlacement::Of(slot, place))
	{
		memcpy(out.scale, place.scale, sizeof(out.scale));
		memcpy(out.position, place.position, sizeof(out.position));
		out.tilt = place.tilt;
		out.turn = place.turn;
		return out;
	}

	Triple(StageImport::FieldOf(id, "Scale"), out.scale);
	Triple(StageImport::FieldOf(id, "Position"), out.position);
	Single(StageImport::FieldOf(id, "ViewRotationX"), out.tilt);
	Single(StageImport::FieldOf(id, "ViewRotationY"), out.turn);

	return out;
}

void Clear(const std::string& folder)
{
	for (const char* pattern : kWritten)
	{
		WIN32_FIND_DATAA found = {};
		const HANDLE walk = FindFirstFileA((folder + "\\" + pattern).c_str(), &found);

		if (walk == INVALID_HANDLE_VALUE)
			continue;

		do
		{
			if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				DeleteFileA((folder + "\\" + found.cFileName).c_str());
		}
		while (FindNextFileA(walk, &found) != 0);

		FindClose(walk);
	}
}

bool WriteFiles(const std::string& folder, const std::vector<BbtagExport::File>& files)
{
	for (const BbtagExport::File& file : files)
	{
		if (!WriteWholeFile(folder + "\\" + file.name, file.data))
			return false;
	}

	return true;
}

bool WriteArchives(const std::string& folder, const BbtagExport::Result& result, FbGameFolder::Game game)
{
	BbtagExport::Archives archives;

	if (!BbtagExport::Package(result, game, result.stage, archives))
		return false;

	CreateDirectoryTree(folder);
	Clear(folder);

	return StageInstall::Write(folder, result.stage, archives);
}

bool Write(const Job& job, const BbtagExport::Result& result)
{
	if (!CreateDirectoryTree(job.folder))
		return false;

	Clear(job.folder);

	if (!WriteWholeFile(job.folder + "\\" + job.stage + kModelSuffix, result.model)
		|| !WriteFiles(job.folder, result.motions) || !WriteFiles(job.folder, result.scripts)
		|| !WriteFiles(job.folder, result.images))
		return false;

	for (const GameFolder& one : kGameFolders)
	{
		if (!WriteArchives(job.folder + "\\" + one.folder, result, one.game))
			return false;
	}

	return true;
}

std::string InstallInto(const Job& job, const BbtagExport::Result& result)
{
	if (job.target.stem.empty())
		return std::string();

	BbtagExport::Archives archives;

	if (!BbtagExport::Package(result, job.game, StageInstall::ModelOf(job.game, job.target), archives))
		return " Could not package it for the game.";

	std::string report;
	StageInstall::Install(job.game, job.target, archives, job.name, report);

	return report;
}

std::string Summary(const Job& job, const BbtagExport::Result& result)
{
	char text[256] = {};
	sprintf_s(text, "Exported %s: %d mesh(es), %d animated, %d texture(s).", job.stage.c_str(),
		result.meshes, result.animated, static_cast<int>(result.images.size()));

	std::string out = text;

	if (!result.missing.empty())
		out += " " + std::to_string(result.missing.size()) + " texture(s) not found.";

	if (!result.foreign.empty())
		out += " " + std::to_string(result.foreign.size()) + " texture(s) are not DDS.";

	if (result.turned)
		out += " View rotation Y is not carried over.";

	return out;
}

void Run(const Job& job)
{
	std::vector<uint8_t> model;

	if (!ReadWholeFile(job.from + "\\" + kModelFile, model))
	{
		Report("Could not read bg.fbx.bin of " + job.name + ".");
		return;
	}

	BbtagExport::Source source;
	source.model.swap(model);
	source.stage = job.stage;
	source.framing = job.framing;
	source.image = [&job](const std::string& name, std::vector<uint8_t>& out)
	{
		return ReadWholeFile(job.from + "\\" + name, out);
	};

	BbtagExport::Result result;
	std::string error;

	if (!BbtagExport::Convert(source, result, error))
	{
		Report("Could not export " + job.name + ": " + error + ".");
		return;
	}

	if (!Write(job, result))
	{
		Report("Could not write into " + job.folder + ".");
		return;
	}

	const std::string summary = Summary(job, result) + InstallInto(job, result);
	LOG("StageExport: %s Folder %s\n", summary.c_str(), job.folder.c_str());
	Report(summary, job.folder);
}

DWORD WINAPI Worker(void* parameter)
{
	Job* const job = static_cast<Job*>(parameter);

	Run(*job);

	delete job;
	InterlockedExchange(&g_busy, 0);

	return 0;
}

bool Launch(int id, FbGameFolder::Game game, const StageInstall::Target& target)
{
	StageLibrary::Entry entry = {};

	if (!StageLibrary::Of(id, entry) || InterlockedCompareExchange(&g_busy, 1, 0) != 0)
		return false;

	Job* const job = new Job();
	job->id = id;
	job->name = entry.name;
	job->from = StageLibrary::FolderOf(id);
	job->stage = Stem(entry.name, id);
	job->folder = GetModRootPath(kExportRoot) + "\\" + job->stage;
	job->framing = FramingOf(id, entry.slot);
	job->game = game;
	job->target = target;

	Report("Exporting " + entry.name + "...");

	const HANDLE thread = CreateThread(nullptr, 0, &Worker, job, 0, nullptr);

	if (thread == nullptr)
	{
		delete job;
		Report("Could not start the export.");
		InterlockedExchange(&g_busy, 0);
		return false;
	}

	SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
	CloseHandle(thread);

	return true;
}

}

bool StageExport::Start(int id)
{
	return Launch(id, FbGameFolder::Game_None, StageInstall::Target());
}

bool StageExport::Install(int id, FbGameFolder::Game game, const StageInstall::Target& target)
{
	if (!StageInstall::Supports(game) || target.stem.empty())
		return false;

	return Launch(id, game, target);
}

bool StageExport::IsBusy()
{
	return InterlockedCompareExchange(&g_busy, 0, 0) != 0;
}

std::string StageExport::StatusText()
{
	std::lock_guard<std::mutex> hold(g_lock);

	return g_status;
}

std::string StageExport::Folder()
{
	std::lock_guard<std::mutex> hold(g_lock);

	return g_folder;
}
