#include "Core/Harness/ImportCommands.h"

#include "Game/Audio/AnnouncerImport.h"
#include "Game/Customize/PortraitImport.h"
#include "Game/Customize/PortraitLayer.h"
#include "Game/Customize/PortraitPicks.h"

#include <cstdio>
#include <cstdlib>

namespace {

std::string Argument(const std::string& line, const std::string& verb)
{
	if (line.size() <= verb.size() + 1)
		return std::string();

	return line.substr(verb.size() + 1);
}

std::string State(bool busy, int progress, const std::string& status)
{
	char text[64] = {};
	sprintf_s(text, "ok %s %d ", busy ? "busy" : "idle", progress);
	return text + status;
}

std::string Announcers(const std::string& line, const std::string& verb)
{
	const std::string picked = Argument(line, verb);
	const std::string folder = picked.empty() ? AnnouncerImport::FindGame() : picked;

	if (!AnnouncerImport::Begin(folder.c_str()))
		return "error " + AnnouncerImport::StatusText();

	return "ok started " + folder;
}

std::string Portraits(const std::string& line, const std::string& verb)
{
	const std::string picked = Argument(line, verb);
	const std::string folder = picked.empty() ? PortraitImport::FindGame() : picked;

	if (!PortraitImport::Begin(folder.c_str()))
		return "error " + PortraitImport::StatusText();

	return "ok started " + folder;
}

std::string PortraitArt(const std::vector<std::string>& words)
{
	if (!PortraitImport::IsInstalled())
		return "error the old portraits are not installed";

	if (words.size() == 2)
		PortraitLayer::WearAll(words[1] == "all");

	if (words.size() == 3)
		PortraitLayer::Wear(atoi(words[1].c_str()), words[2] == "on");

	std::vector<int> worn;

	for (int chara : PortraitLayer::Available())
	{
		if (PortraitLayer::IsWorn(chara))
			worn.push_back(chara);
	}

	return "ok old " + PortraitPicks::Joined(worn);
}

}

bool ImportCommands::Execute(const std::vector<std::string>& words, const std::string& line,
	std::string& reply)
{
	const std::string& verb = words[0];

	if (verb == "announcers")
	{
		reply = Announcers(line, verb);
		return true;
	}

	if (verb == "announcers?")
	{
		reply = State(AnnouncerImport::IsBusy(), AnnouncerImport::Progress(), AnnouncerImport::StatusText());
		return true;
	}

	if (verb == "portraits")
	{
		reply = Portraits(line, verb);
		return true;
	}

	if (verb == "portraits?")
	{
		reply = State(PortraitImport::IsBusy(), PortraitImport::Progress(), PortraitImport::StatusText());
		return true;
	}

	if (verb == "portraitart")
	{
		reply = PortraitArt(words);
		return true;
	}

	return false;
}
