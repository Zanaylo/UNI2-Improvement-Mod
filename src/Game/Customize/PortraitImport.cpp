#include "Game/Customize/PortraitImport.h"

#include "Core/BackgroundJob.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Customize/PortraitArt.h"
#include "Game/Customize/PortraitLayer.h"
#include "Game/Files/DataArchive.h"
#include "Game/Files/FbGameFolder.h"
#include "Game/Files/GameArchive.h"
#include "Game/Files/ModFiles.h"
#include "Game/Files/ModPacks.h"
#include "Game/Files/SteamLibrary.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* kNotTheGame = "that folder is not UNDER NIGHT IN-BIRTH Exe:Late[st]";
constexpr const char* kStarting = "reading the old portraits...";
constexpr const char* kInstalls[] = { "UNDER NIGHT In-Birth Exe Late[st]", "UNDER NIGHT IN-BIRTH Exe Late[cl-r]" };
constexpr const char* kOldPack = "uni-old-portraits";
constexpr const char* kArchiveFolder = "d\\";
constexpr const char* kGaugeFolder = "grpdat\\Cockpit\\chara";
constexpr const char* kTheirGauge = "gauge_%s_1P.dds";
constexpr const char* kOurGauge = "gauge_chr%03d.dds";

struct Fighter
{
	const char* tag;
	int chara;
};

constexpr Fighter kFighters[] = {
	{ "hyd", 0 }, { "lin", 1 }, { "wal", 2 }, { "car", 3 }, { "ori", 4 }, { "gor", 5 }, { "mer", 6 },
	{ "vat", 7 }, { "set", 8 }, { "yuz", 9 }, { "hil", 10 }, { "elt", 11 }, { "nan", 12 }, { "bya", 13 },
	{ "aka", 14 }, { "cha", 15 }, { "wag", 16 }, { "enk", 17 }, { "lnd", 18 }, { "mik", 21 }, { "pho", 24 },
};

constexpr int kFighterCount = static_cast<int>(sizeof(kFighters) / sizeof(kFighters[0]));

struct Screen
{
	PortraitArt::Screen screen;
	const char* folder;
	const char* oursFile;
};

constexpr Screen kScreens[] = {
	{ PortraitArt::Screen_Select, "grpdat\\CSel\\chara", "cs_chr%03d.pat" },
	{ PortraitArt::Screen_Versus, "grpdat\\VsScreen", "vs_demo_chr%03d.pat" },
	{ PortraitArt::Screen_Winner, "grpdat\\Winner", "win_ch_chr%03d.pat" },
	{ PortraitArt::Screen_Menu, "grpdat\\MainMenuCS\\menucha", "menucha_chr%03d.pat" },
};

BackgroundJob g_job("PortraitImport");

void Status(const char* text)
{
	g_job.SetStatus(text);
}

std::string Combine(const std::string& folder, const std::string& name)
{
	return folder.empty() || folder.back() == '\\' ? folder + name : folder + "\\" + name;
}

std::string Named(const char* pattern, const char* tag)
{
	char name[MAX_PATH] = {};
	sprintf_s(name, pattern, tag);
	return name;
}

std::string Numbered(const char* pattern, int chara)
{
	char name[MAX_PATH] = {};
	sprintf_s(name, pattern, chara);
	return name;
}

std::vector<uint8_t> TheirFile(const GameArchive& theirs, const char* folder, const std::string& file)
{
	std::vector<uint8_t> bytes;
	theirs.Read(folder, file.c_str(), bytes);
	return bytes;
}

bool TakeSources(const GameArchive& theirs, const char* tag, PortraitArt::Sources& out)
{
	const bool versus = PortraitArt::TakeVersus(TheirFile(theirs, "grpdat\\VsScreen", Named("vs_demo_%s.pat", tag)), out);
	const bool face = PortraitArt::TakeFace(TheirFile(theirs, "grpdat\\CSel\\chara", Named("cs_%s.pat", tag)), out);
	const bool winner = PortraitArt::TakeWinner(TheirFile(theirs, "grpdat\\Winner", Named("win_ch_%s.pat", tag)), out);
	const bool menu = PortraitArt::TakeMenu(TheirFile(theirs, "grpdat\\MainMenuCS\\menucha", Named("menucha_%s.pat", tag)), out);

	LOG("PortraitImport: %s versus %d face %d winner %d menu %d", tag, versus, face, winner, menu);

	return versus || winner || menu;
}

int Repaint(const Fighter& fighter, const PortraitArt::Sources& sources, const std::string& root)
{
	int written = 0;

	for (const Screen& screen : kScreens)
	{
		const std::string file = Numbered(screen.oursFile, fighter.chara);
		std::vector<uint8_t> ours;
		std::vector<uint8_t> painted;

		if (!DataArchive::Read(screen.folder, file.c_str(), ours) ||
			!PortraitArt::Repaint(ours, screen.screen, sources, painted))
		{
			LOG("PortraitImport: %s was left as it is", file.c_str());
			continue;
		}

		const std::string target = Combine(Combine(root, screen.folder), file);
		CreateDirectoryTree(Combine(root, screen.folder));

		if (WriteWholeFile(target, painted))
			++written;
	}

	return written;
}

int RepaintGauge(const GameArchive& theirs, const Fighter& fighter, const std::string& root)
{
	const std::string file = Numbered(kOurGauge, fighter.chara);
	std::vector<uint8_t> ours;
	DdsImage::Image theirArt;
	DdsImage::Image ourArt;
	DdsImage::Image painted;

	if (!DdsImage::Decode(TheirFile(theirs, kGaugeFolder, Named(kTheirGauge, fighter.tag)), theirArt)
		|| !DataArchive::Read(kGaugeFolder, file.c_str(), ours) || !DdsImage::Decode(ours, ourArt)
		|| !PortraitArt::RepaintGauge(theirArt, ourArt, painted))
	{
		LOG("PortraitImport: %s was left as it is", file.c_str());
		return 0;
	}

	CreateDirectoryTree(Combine(root, kGaugeFolder));

	return WriteWholeFile(Combine(Combine(root, kGaugeFolder), file), DdsImage::EncodeArgb(painted)) ? 1 : 0;
}

bool Run(const std::string& folder)
{
	GameArchive theirs;

	if (!theirs.Open(Combine(folder, kArchiveFolder)))
	{
		Status("that copy of the game has no data this mod can read");
		return false;
	}

	const std::string root = PortraitLayer::Folder();
	RemoveDirectoryTree(root);
	CreateDirectoryTree(root);

	int fighters = 0;
	int files = 0;

	for (int i = 0; i < kFighterCount; ++i)
	{
		g_job.SetProgress(5 + (i * 90) / kFighterCount);

		PortraitArt::Sources sources;
		const int written = (TakeSources(theirs, kFighters[i].tag, sources) ? Repaint(kFighters[i], sources, root) : 0)
			+ RepaintGauge(theirs, kFighters[i], root);
		files += written;
		fighters += written > 0 ? 1 : 0;
	}

	if (files == 0)
	{
		Status("no portrait could be taken from that copy of the game");
		RemoveDirectoryTree(root);
		return false;
	}

	RemoveDirectoryTree(Combine(ModPacks::Root(), kOldPack));

	char done[192] = {};
	sprintf_s(done, "%d characters now wear their old portraits (%d screens). Tick or untick them below.",
		fighters, files);
	Status(done);

	return true;
}


}

std::string PortraitImport::FindGame()
{
	for (const char* install : kInstalls)
	{
		const std::string folder = SteamLibrary::GameFolder(install);

		if (IsGame(folder.c_str()))
			return folder;
	}

	return std::string();
}

bool PortraitImport::IsGame(const char* folder)
{
	return folder != nullptr && folder[0] != '\0' && FbGameFolder::Detect(folder) == FbGameFolder::Game_UNI;
}

bool PortraitImport::Begin(const char* folder)
{
	if (!IsGame(folder))
	{
		Status(kNotTheGame);
		return false;
	}

	if (g_job.IsBusy())
		return false;

	Status(kStarting);

	const std::string picked = folder;
	return g_job.Start([picked]() { return Run(picked); });
}

void PortraitImport::Update()
{
	if (!g_job.ConsumeFinished())
		return;

	ModPacks::Scan();
	ModFiles::Rescan();
}

bool PortraitImport::IsBusy()
{
	return g_job.IsBusy();
}

int PortraitImport::Progress()
{
	return g_job.Progress();
}

std::string PortraitImport::StatusText()
{
	return g_job.Status();
}

bool PortraitImport::IsInstalled()
{
	return !PortraitLayer::Available().empty();
}
