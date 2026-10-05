#include "Core/Harness/ImportCommands.h"

#include "Game/Audio/AnnouncerImport.h"
#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitChoices.h"
#include "Game/Customize/PortraitCompose.h"
#include "Game/Customize/PortraitDownload.h"

#include <cstdio>
#include <cstdlib>

namespace {

constexpr const char* kGameArt = "none";

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

std::string Portrait(const std::vector<std::string>& words)
{
	if (words.size() != 3)
		return "error portrait <chara> <art id|none>";

	const int chara = atoi(words[1].c_str());
	const std::string id = words[2] == kGameArt ? std::string() : words[2];
	const PortraitCatalog::Art* const art = PortraitCatalog::Find(id);

	if (!id.empty() && (art == nullptr || art->chara != chara))
		return "error no such art for that character";

	PortraitCompose::Wear(chara, id);
	return "ok " + words[1] + " " + words[2];
}

std::string Worn(const std::vector<std::string>& words)
{
	if (words.size() != 2)
		return "error portraitworn <chara>";

	const std::string id = PortraitChoices::Of(atoi(words[1].c_str()));
	return "ok " + (id.empty() ? std::string(kGameArt) : id);
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

	if (verb == "portrait")
	{
		reply = Portrait(words);
		return true;
	}

	if (verb == "portrait?")
	{
		reply = State(PortraitCompose::IsBusy(), PortraitCompose::Progress(), PortraitCompose::StatusText());
		return true;
	}

	if (verb == "portraitworn")
	{
		reply = Worn(words);
		return true;
	}

	if (verb == "portraitfetch")
	{
		reply = PortraitDownload::BeginAll() ? "ok started" : "error already downloading";
		return true;
	}

	if (verb == "portraitfetch?")
	{
		reply = State(PortraitDownload::IsBusy(), PortraitDownload::Progress(), PortraitDownload::StatusText());
		return true;
	}

	return false;
}
