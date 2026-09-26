#include "Core/Harness/StageCommands.h"

#include "Core/Harness/CleanFrame.h"
#include "Core/utils.h"
#include "D3D9/Draw/DrawTrace.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Stages/StageImport.h"
#include "Game/Stages/StageLibrary.h"

#include <cstdint>
#include <cstdlib>

namespace {

using Words = std::vector<std::string>;

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

struct Command
{
	const char* verb;
	std::string (*run)(const Words&, const std::string&);
};

constexpr Command kCommands[] = {
	{ "stage", &RunStage },
	{ "clean", &RunClean },
	{ "import", &RunImport },
	{ "importing", &RunImporting },
	{ "library", &RunLibrary },
	{ "remove", &RunRemove },
	{ "trace", &RunTrace },
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
