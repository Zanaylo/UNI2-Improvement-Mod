#include "Core/Harness/StageCommands.h"

#include "Core/Harness/CleanFrame.h"
#include "Core/ShellOpen.h"
#include "Core/utils.h"
#include "D3D9/Draw/DrawTrace.h"
#include "D3D9/Draw/TargetDump.h"
#include "Game/Battle/GameRestart.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Stages/StageExport.h"
#include "Game/Stages/StageImport.h"
#include "Game/Stages/StageInstall.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Stages/StagePlacement.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using Words = std::vector<std::string>;

constexpr const char* kBbcfWord = "BBCF";

std::string Rest(const std::string& line)
{
	const size_t space = line.find(' ');

	return space == std::string::npos ? std::string() : line.substr(space + 1);
}

void* At(uintptr_t rva)
{
	return reinterpret_cast<void*>(RvaToAddress(rva));
}

std::string RunStage(const Words& words, const std::string&)
{
	uint32_t saved = 0;

	if (!TryReadDword(At(GameOffsets::kTrainingStageSaved), saved))
		return "error the training stage could not be read";

	if (words.size() < 2)
		return "ok " + std::to_string(saved);

	const uint32_t number = static_cast<uint32_t>(atoi(words[1].c_str()));

	if (number == 0)
		return "error stage needs a stage number";

	if (!TryWriteDword(At(GameOffsets::kTrainingStageSaved), number) ||
		!TryWriteDword(At(GameOffsets::kTrainingStageLive), number))
	{
		return "error the training stage could not be written";
	}

	return "ok " + std::to_string(saved) + " -> " + std::to_string(number);
}

std::string RunClean(const Words& words, const std::string&)
{
	if (words.size() >= 2)
		CleanFrame::SetOn(words[1] == "on");

	return CleanFrame::IsOn() ? "ok on" : "ok off";
}

std::string RunImport(const Words&, const std::string& line)
{
	const std::string folder = Rest(line);

	if (folder.empty())
		return "error import needs a game folder";

	if (!StageImport::Scan(folder.c_str()))
		return std::string("error ") + StageImport::StatusText();

	std::vector<int> indices;
	std::vector<const char*> names;

	for (int i = 0; i < StageImport::OfferCount(); ++i)
	{
		indices.push_back(i);
		names.push_back(StageImport::OfferAt(i)->name.c_str());
	}

	if (!StageImport::InstallMany(indices.data(), names.data(), static_cast<int>(indices.size())))
		return std::string("error ") + StageImport::StatusText();

	return "ok " + std::to_string(indices.size()) + " queued";
}

bool Wanted(const std::string& folder, const std::string& stems)
{
	size_t start = 0;

	while (start <= stems.size())
	{
		const size_t comma = stems.find(',', start);
		const std::string stem = stems.substr(start, comma == std::string::npos ? std::string::npos : comma - start);

		if (!stem.empty() && folder.size() >= stem.size()
			&& folder.compare(folder.size() - stem.size(), stem.size(), stem) == 0)
		{
			return true;
		}

		if (comma == std::string::npos)
			return false;

		start = comma + 1;
	}

	return false;
}

std::string RunImportSome(const Words&, const std::string& line)
{
	const std::string rest = Rest(line);
	const size_t bar = rest.find('|');

	if (bar == std::string::npos)
		return "error importsome needs <game folder>|<stage,stage>";

	const std::string folder = rest.substr(0, bar);
	const std::string stems = rest.substr(bar + 1);

	if (!StageImport::Scan(folder.c_str()))
		return std::string("error ") + StageImport::StatusText();

	std::vector<int> indices;
	std::vector<const char*> names;

	for (int i = 0; i < StageImport::OfferCount(); ++i)
	{
		if (!Wanted(StageImport::OfferAt(i)->folder, stems))
			continue;

		indices.push_back(i);
		names.push_back(StageImport::OfferAt(i)->name.c_str());
	}

	if (indices.empty())
		return "error none of those stages is offered";

	if (!StageImport::InstallMany(indices.data(), names.data(), static_cast<int>(indices.size())))
		return std::string("error ") + StageImport::StatusText();

	return "ok " + std::to_string(indices.size()) + " queued";
}

std::string Described(const StagePlacement::Place& place)
{
	char text[256] = {};
	sprintf_s(text, "%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f", place.scale[0], place.position[0],
		place.position[1], place.position[2], place.fov, place.horizon, place.tilt, place.turn);

	return text;
}

std::string RunPlace(const Words& words, const std::string&)
{
	const int stage = StagePlacement::Current();
	StagePlacement::Place place = {};

	if (stage < 0 || !StagePlacement::Of(stage, place))
		return "error no stage is placed";

	if (words.size() == 2 && words[1] == "reset")
	{
		StagePlacement::Forget(stage);
		return "ok reset";
	}

	if (words.size() < 9)
		return "ok " + Described(place);

	const float scale = static_cast<float>(atof(words[1].c_str()));

	for (float& axis : place.scale)
		axis = scale;

	for (int k = 0; k < 3; ++k)
		place.position[k] = static_cast<float>(atof(words[2 + k].c_str()));

	place.fov = static_cast<float>(atof(words[5].c_str()));
	place.horizon = static_cast<float>(atof(words[6].c_str()));
	place.tilt = static_cast<float>(atof(words[7].c_str()));
	place.turn = static_cast<float>(atof(words[8].c_str()));
	StagePlacement::Set(stage, place);

	return "ok " + Described(place);
}

std::string RunImporting(const Words&, const std::string&)
{
	if (StageImport::IsBusy())
		return "ok busy " + std::to_string(StageImport::Progress());

	return std::string("ok idle ") + StageImport::StatusText();
}

std::string RunLibrary(const Words&, const std::string&)
{
	std::vector<StageLibrary::Entry> entries;
	StageLibrary::Snapshot(entries);

	std::string reply = "ok " + std::to_string(entries.size());

	for (const StageLibrary::Entry& entry : entries)
	{
		reply += "|" + std::to_string(entry.id) + "\t" + std::to_string(entry.slot) + "\t" +
			entry.game + "\t" + entry.folder + "\t" + entry.name;
	}

	return reply;
}

std::string RunRemove(const Words& words, const std::string&)
{
	if (words.size() < 2)
		return "error remove needs a stage id";

	if (!StageImport::Remove(atoi(words[1].c_str())))
		return std::string("error ") + StageImport::StatusText();

	return "ok removing";
}

std::string RunTrace(const Words&, const std::string&)
{
	DrawTrace::Arm();
	return "ok armed";
}

std::string RunDumpTargets(const Words&, const std::string&)
{
	TargetDump::Arm();
	return "ok armed";
}

std::string RunOpen(const Words&, const std::string& line)
{
	const std::string target = Rest(line);

	if (target.empty())
		return "error open needs a path";

	ShellOpen::Open(target);
	return "ok opening";
}

std::string RunDumpAfter(const Words& words, const std::string&)
{
	if (words.size() < 2)
		return "error dumpafter needs a draw number";

	TargetDump::ArmAfterDraw(atoi(words[1].c_str()));
	return "ok armed";
}

FbGameFolder::Game ExportGameOf(const std::string& word)
{
	return word == kBbcfWord ? FbGameFolder::Game_BBCF : FbGameFolder::Game_BBTAG;
}

bool TargetOf(const std::string& word, StageInstall::Target& out)
{
	const size_t slash = word.find_last_of('\\');

	if (slash == std::string::npos)
		return false;

	out = { word.substr(0, slash), word.substr(slash + 1) };

	return true;
}

std::string RunInstall(const Words& words, const std::string&)
{
	StageInstall::Target target;

	if (words.size() < 4 || !TargetOf(words[3], target))
		return "error install needs <stage id> <BBTAG|BBCF> <folder\\stem>";

	if (!StageExport::Install(atoi(words[1].c_str()), ExportGameOf(words[2]), target))
		return "error " + StageExport::StatusText();

	return "ok installing";
}

std::string RunSoftReset(const Words&, const std::string&)
{
	return std::string(GameRestart::SoftReset() ? "ok " : "error ") + GameRestart::StatusText();
}

std::string RunExporting(const Words&, const std::string&)
{
	return std::string(StageExport::IsBusy() ? "ok busy " : "ok idle ") + StageExport::StatusText();
}

std::string RunRestore(const Words& words, const std::string&)
{
	StageInstall::Target target;

	if (words.size() < 3 || !TargetOf(words[2], target))
		return "error restore needs <BBTAG|BBCF> <folder\\stem>";

	std::string report;

	return (StageInstall::Restore(ExportGameOf(words[1]), target, report) ? "ok " : "error ") + report;
}

struct Command
{
	const char* verb;
	std::string (*run)(const Words&, const std::string&);
};

constexpr Command kCommands[] = {
	{ "stage", &RunStage },
	{ "clean", &RunClean },
	{ "import", &RunImport },
	{ "importsome", &RunImportSome },
	{ "place", &RunPlace },
	{ "importing", &RunImporting },
	{ "library", &RunLibrary },
	{ "remove", &RunRemove },
	{ "trace", &RunTrace },
	{ "dumptargets", &RunDumpTargets },
	{ "dumpafter", &RunDumpAfter },
	{ "open", &RunOpen },
	{ "install", &RunInstall },
	{ "exporting", &RunExporting },
	{ "restore", &RunRestore },
	{ "softreset", &RunSoftReset },
};

}

bool StageCommands::Execute(const Words& words, const std::string& line, std::string& reply)
{
	for (const Command& command : kCommands)
	{
		if (words[0] != command.verb)
			continue;

		reply = command.run(words, line);
		return true;
	}

	return false;
}
