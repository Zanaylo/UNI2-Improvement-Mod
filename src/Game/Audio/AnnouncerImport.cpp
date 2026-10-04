#include "Game/Audio/AnnouncerImport.h"

#include "Core/BackgroundJob.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Audio/AnnouncerLines.h"
#include "Game/Audio/AnnouncerRoster.h"
#include "Game/Audio/AudioFile.h"
#include "Game/Customize/AnnouncerArt.h"
#include "Game/Customize/AnnouncerList.h"
#include "Game/Files/DataArchive.h"
#include "Game/Files/FbGameFolder.h"
#include "Game/Files/MbtlArchive.h"
#include "Game/Files/ModFiles.h"
#include "Game/Files/ModPacks.h"
#include "Game/Files/SteamLibrary.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr const char* kNotTheGame = "that folder is not MELTY BLOOD TYPE LUMINA";
constexpr const char* kStarting = "reading MELTY BLOOD TYPE LUMINA...";
constexpr const char* kInstall = "MELTY BLOOD TYPE LUMINA";
constexpr const char* kPackFolder = "mbtl-announcers";

constexpr const char* kTheirAnnouncers = "se/normal_se/announce/";
constexpr const char* kTheirList = "se/normal_se_list.txt";
constexpr const char* kTheirPortrait = "grpdat/CSel/chara/cs_%s.pat";
constexpr const char* kTheirVoice = "se/normal_se/announce/%s/%s.wav";

constexpr const char* kOurListFolder = "se";
constexpr const char* kOurListFile = "normal_se_list.txt";
constexpr const char* kOurMenuVoices = "se\\mainmenu_se\\chr000";
constexpr const char* kCustomize = "grpdat\\Customize";
constexpr const char* kMenuFolder = "grpdat\\MainMenuCS\\menucha";
constexpr const char* kIconPat = "customize_icon00.pat";
constexpr const char* kAnnouncerList = "AnnounceCharaList.csv";
constexpr const char* kIconLayout = "CustomizeMainMenuCharaSelAnim.ini";
constexpr const char* kCharacterMenu = "menucha_chr000.pat";
constexpr const char* kSystemMenu = "menucha_chr100.pat";
constexpr const char* kArtFolder = "grpdat";

constexpr const char* kRoundCallFolder = "se\\normal_se\\announce\\chr%03d\\%s";
constexpr const char* kMenuCallFolder = "se\\mainmenu_se\\chr%03d\\%s";
constexpr const char* kMenuPat = "grpdat\\MainMenuCS\\menucha\\menucha_chr%03d.pat%s";
constexpr const char* kVoiceExtension = ".wav";

constexpr const char* kNameSuffix = " (MBTL)";

constexpr int kSilenceRate = 48000;
constexpr int kSilenceFrames = 2400;

constexpr int kReadShare = 5;
constexpr int kVoicesShare = 80;

struct Announcer
{
	std::string theirFolder;
	std::string name;
	int number;
	bool hasFace;
	DdsImage::Image portrait;
};

struct Calls
{
	std::vector<AnnouncerLines::Pair> round;
	std::vector<AnnouncerLines::Pair> menu;
};

BackgroundJob g_job("AnnouncerImport");

void Status(const char* text)
{
	g_job.SetStatus(text);
}

void Advance(int done, int total, int first, int share)
{
	if (total <= 0)
		return;

	g_job.SetProgress(first + (done * share) / total);
}

std::string Combine(const std::string& folder, const std::string& name)
{
	return folder.empty() || folder.back() == '\\' ? folder + name : folder + "\\" + name;
}

std::string Formatted(const char* pattern, int number, const char* leaf)
{
	char text[MAX_PATH] = {};
	sprintf_s(text, pattern, number, leaf);
	return text;
}

std::string TheirFolderOf(const std::string& name)
{
	const size_t first = strlen(kTheirAnnouncers);
	const size_t slash = name.find('/', first);

	return slash == std::string::npos ? std::string() : name.substr(first, slash - first);
}

std::vector<uint8_t> OurFile(const char* folder, const char* file)
{
	std::vector<uint8_t> bytes;
	DataArchive::Read(folder, file, bytes);
	return bytes;
}

std::string OurText(const char* folder, const char* file)
{
	const std::vector<uint8_t> bytes = OurFile(folder, file);
	return std::string(bytes.begin(), bytes.end());
}

std::vector<std::string> OurMenuStems()
{
	std::vector<std::string> files;
	DataArchive::List(kOurMenuVoices, files);

	std::vector<std::string> stems;

	for (const std::string& file : files)
		stems.push_back(file.substr(0, file.find_last_of('.')));

	return stems;
}

std::vector<Announcer> Announcers(const MbtlArchive& theirs)
{
	std::vector<std::string> names;
	theirs.List(kTheirAnnouncers, names);

	std::set<std::string> present;

	for (const std::string& name : names)
		present.insert(TheirFolderOf(name));

	std::vector<Announcer> out;

	for (int i = 0; i < AnnouncerRoster::Count(); ++i)
	{
		const AnnouncerRoster::Speaker& speaker = AnnouncerRoster::At(i);

		if (present.count(speaker.folder) == 0)
			continue;

		Announcer announcer;
		announcer.theirFolder = speaker.folder;
		announcer.name = std::string(speaker.name) + kNameSuffix;
		announcer.number = AnnouncerRoster::NumberOf(speaker.folder);
		announcer.hasFace = false;
		out.push_back(announcer);
	}

	return out;
}

void TakeFace(const MbtlArchive& theirs, Announcer& announcer)
{
	char name[MAX_PATH] = {};
	sprintf_s(name, kTheirPortrait, announcer.theirFolder.c_str());

	std::vector<uint8_t> pat;
	announcer.hasFace = theirs.Read(name, pat) && AnnouncerArt::Portrait(pat, announcer.portrait);

	if (!announcer.hasFace)
		LOG("AnnouncerImport: %s has no select portrait, it gets the system card", announcer.name.c_str());
}

std::vector<uint8_t> SilentWave()
{
	const uint32_t data = kSilenceFrames * 2;
	std::vector<uint8_t> wave(44 + data, 0);

	const auto put = [&wave](size_t at, uint32_t value, int bytes) {
		for (int i = 0; i < bytes; ++i)
			wave[at + i] = static_cast<uint8_t>(value >> (8 * i));
	};

	memcpy(&wave[0], "RIFF", 4);
	put(4, 36 + data, 4);
	memcpy(&wave[8], "WAVEfmt ", 8);
	put(16, 16, 4);
	put(20, 1, 2);
	put(22, 1, 2);
	put(24, kSilenceRate, 4);
	put(28, kSilenceRate * 2, 4);
	put(32, 2, 2);
	put(34, 16, 2);
	memcpy(&wave[36], "data", 4);
	put(40, data, 4);

	return wave;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string FolderOf(const std::string& path)
{
	return path.substr(0, path.find_last_of('\\'));
}

bool WriteBytes(const std::string& path, const std::vector<uint8_t>& bytes)
{
	CreateDirectoryTree(FolderOf(path));
	return WriteWholeFile(path, bytes);
}

bool WriteText(const std::string& path, const std::string& text)
{
	return WriteBytes(path, std::vector<uint8_t>(text.begin(), text.end()));
}

bool WriteVoice(const std::string& stemPath, const std::vector<uint8_t>& wave)
{
	char why[160] = {};
	const std::string path = stemPath + kVoiceExtension;

	CreateDirectoryTree(FolderOf(stemPath));

	if (AudioFile::ConvertBytesToOgg(wave, path, why, sizeof(why)))
		return true;

	LOG("AnnouncerImport: %s %s", path.c_str(), why);
	return false;
}

std::string LastCallOf(const Announcer& announcer, const Calls& calls, const std::string& root)
{
	const AnnouncerLines::Pair& last = calls.menu.empty() ? calls.round.back() : calls.menu.back();
	const char* const pattern = calls.menu.empty() ? kRoundCallFolder : kMenuCallFolder;

	return Combine(root, Formatted(pattern, announcer.number, last.ours.c_str())) + kVoiceExtension;
}

int WriteCalls(const MbtlArchive& theirs, const Announcer& announcer,
	const std::vector<AnnouncerLines::Pair>& pairs, const char* folderPattern, const std::string& root,
	const std::vector<uint8_t>& silence)
{
	int spoken = 0;

	for (const AnnouncerLines::Pair& pair : pairs)
	{
		std::vector<uint8_t> wave;

		if (!pair.theirs.empty())
		{
			char name[MAX_PATH] = {};
			sprintf_s(name, kTheirVoice, announcer.theirFolder.c_str(), pair.theirs.c_str());
			theirs.Read(name, wave);
		}

		const bool said = !wave.empty();
		const std::string stem = Combine(root, Formatted(folderPattern, announcer.number, pair.ours.c_str()));

		if (WriteVoice(stem, said ? wave : silence) && said)
			++spoken;
	}

	return spoken;
}

void WriteVoices(const MbtlArchive& theirs, const std::vector<Announcer>& announcers, const Calls& calls,
	const std::string& root)
{
	const std::vector<uint8_t> silence = SilentWave();
	int done = 0;

	for (const Announcer& announcer : announcers)
	{
		Advance(done++, static_cast<int>(announcers.size()), kReadShare, kVoicesShare);

		if (Exists(LastCallOf(announcer, calls, root)))
			continue;

		const int round = WriteCalls(theirs, announcer, calls.round, kRoundCallFolder, root, silence);
		const int menu = WriteCalls(theirs, announcer, calls.menu, kMenuCallFolder, root, silence);

		LOG("AnnouncerImport: %s says %d of %d round calls and %d of %d menu calls",
			announcer.name.c_str(), round, static_cast<int>(calls.round.size()), menu,
			static_cast<int>(calls.menu.size()));
	}
}

std::vector<Announcer> ChosenOf(const std::vector<Announcer>& announcers)
{
	const std::vector<std::string> chosen = AnnouncerRoster::Chosen();
	std::vector<Announcer> out;

	for (const Announcer& announcer : announcers)
	{
		if (std::find(chosen.begin(), chosen.end(), announcer.theirFolder) != chosen.end())
			out.push_back(announcer);
	}

	return out;
}

bool WriteMenus(const std::vector<Announcer>& chosen, const std::string& root)
{
	const std::vector<uint8_t> characterMenu = OurFile(kMenuFolder, kCharacterMenu);
	const std::vector<uint8_t> systemMenu = OurFile(kMenuFolder, kSystemMenu);

	if (systemMenu.empty())
		return false;

	for (const Announcer& announcer : chosen)
	{
		std::vector<uint8_t> menu;

		if (!announcer.hasFace || !AnnouncerArt::Menu(characterMenu, systemMenu, announcer.portrait, menu))
			menu = systemMenu;

		if (!WriteBytes(Combine(root, Formatted(kMenuPat, announcer.number, "")), menu))
			return false;
	}

	return true;
}

std::vector<AnnouncerList::Entry> Entries(const std::vector<Announcer>& chosen, const std::string& csv)
{
	const int firstCell = AnnouncerList::RowCount(csv);
	const std::vector<int> saveIds = AnnouncerList::FreeSaveIds(csv, static_cast<int>(chosen.size()));
	std::vector<AnnouncerList::Entry> entries;

	for (size_t i = 0; i < chosen.size() && i < saveIds.size(); ++i)
	{
		const Announcer& announcer = chosen[i];
		char folder[16] = {};
		sprintf_s(folder, "chr%03d", announcer.number);

		entries.push_back(AnnouncerList::Entry{ announcer.number, announcer.name, folder,
			std::string("icon_") + folder, saveIds[i], firstCell + static_cast<int>(i) });
	}

	return entries;
}

bool WriteIcons(const std::vector<Announcer>& chosen, const std::vector<AnnouncerList::Entry>& entries,
	const std::string& root)
{
	std::vector<AnnouncerArt::Face> faces;

	for (size_t i = 0; i < chosen.size() && i < entries.size(); ++i)
		faces.push_back(AnnouncerArt::Face{ chosen[i].number, chosen[i].portrait });

	std::vector<uint8_t> icons;

	return AnnouncerArt::Icons(OurFile(kCustomize, kIconPat), faces, icons) &&
		WriteBytes(Combine(root, Combine(kCustomize, kIconPat)), icons);
}

bool WriteChoice(const MbtlArchive& theirs, std::vector<Announcer> chosen, const std::string& root)
{
	const std::string csv = OurText(kCustomize, kAnnouncerList);
	const std::string layout = OurText(kCustomize, kIconLayout);

	RemoveDirectoryTree(Combine(root, kArtFolder));

	if (chosen.empty())
		return true;

	if (csv.empty() || layout.empty())
		return false;

	for (Announcer& announcer : chosen)
		TakeFace(theirs, announcer);

	const std::vector<AnnouncerList::Entry> entries = Entries(chosen, csv);

	return WriteMenus(chosen, root) && WriteIcons(chosen, entries, root) &&
		WriteText(Combine(root, Combine(kCustomize, kAnnouncerList)), AnnouncerList::WithRows(csv, entries)) &&
		WriteText(Combine(root, Combine(kCustomize, kIconLayout)), AnnouncerList::WithIcons(layout, entries));
}

void WriteDescription(const std::string& root, int count, int shown)
{
	char text[512] = {};
	sprintf_s(text, "[Mod]\r\nName    = MELTY BLOOD TYPE LUMINA announcers\r\nAuthor  = \r\n"
		"Version = 1\r\nNote    = %d announcers taken from your copy of MBTL, %d of them in "
		"Customize, Announcer Character.\r\n", count, shown);

	WriteText(Combine(root, "mod.ini"), text);
}

bool ReadCalls(const MbtlArchive& theirs, Calls& out)
{
	std::vector<uint8_t> theirList;
	theirs.Read(kTheirList, theirList);

	const AnnouncerLines::Lines theirLines = AnnouncerLines::Parse(std::string(theirList.begin(), theirList.end()));
	out.round = AnnouncerLines::RoundCalls(AnnouncerLines::Parse(OurText(kOurListFolder, kOurListFile)), theirLines);
	out.menu = AnnouncerLines::MenuCalls(OurMenuStems(), theirLines);

	return !theirLines.empty() && !out.round.empty();
}

bool Run(const std::string& folder)
{
	MbtlArchive theirs;

	if (!theirs.Open(folder))
	{
		Status(theirs.Problem());
		return false;
	}

	Calls calls;

	if (!ReadCalls(theirs, calls))
	{
		Status("the announcer lists of the two games could not be read");
		return false;
	}

	const std::vector<Announcer> announcers = Announcers(theirs);

	if (announcers.empty())
	{
		Status("that copy of MBTL has no announcer voices");
		return false;
	}

	g_job.SetProgress(kReadShare);
	Status("converting the voices...");

	const std::string root = AnnouncerImport::PackFolder();
	WriteVoices(theirs, announcers, calls, root);

	Status("drawing the icons...");
	const std::vector<Announcer> chosen = ChosenOf(announcers);

	if (!WriteChoice(theirs, chosen, root))
	{
		Status("this game's announcer menu could not be extended");
		return false;
	}

	WriteDescription(root, static_cast<int>(announcers.size()), static_cast<int>(chosen.size()));

	char done[192] = {};
	sprintf_s(done, "%d announcers are in Customize, Announcer Character. Restart the game to see them.",
		static_cast<int>(chosen.size()));
	Status(done);

	return true;
}


}

std::string AnnouncerImport::FindGame()
{
	const std::string folder = SteamLibrary::GameFolder(kInstall);

	return IsGame(folder.c_str()) ? folder : std::string();
}

bool AnnouncerImport::IsGame(const char* folder)
{
	return folder != nullptr && FbGameFolder::Detect(folder) == FbGameFolder::Game_MBTL;
}

bool AnnouncerImport::Begin(const char* folder)
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

void AnnouncerImport::Update()
{
	if (!g_job.ConsumeFinished())
		return;

	ModPacks::Scan();
	ModFiles::Rescan();
}

bool AnnouncerImport::IsBusy()
{
	return g_job.IsBusy();
}

int AnnouncerImport::Progress()
{
	return g_job.Progress();
}

std::string AnnouncerImport::StatusText()
{
	return g_job.Status();
}

std::string AnnouncerImport::PackFolder()
{
	return Combine(ModPacks::Root(), kPackFolder);
}
