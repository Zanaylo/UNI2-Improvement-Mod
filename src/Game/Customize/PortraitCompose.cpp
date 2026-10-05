#include "Game/Customize/PortraitCompose.h"

#include "Core/BackgroundJob.h"
#include "Core/Config/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitChoices.h"
#include "Game/Customize/PortraitDownload.h"
#include "Game/Customize/PortraitFrames.h"
#include "Game/Customize/PortraitLibrary.h"
#include "Game/Customize/PortraitPainter.h"
#include "Game/Files/DataArchive.h"
#include "Game/Files/ModFiles.h"
#include "Game/Tables/CharaTables.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace {

constexpr const char* kScreensFolder = "screens";
constexpr const char* kCardsFolder = "cards";
constexpr const char* kCardFile = "chr%03d.dds";
constexpr const char* kLegacyScreens = "grpdat";
constexpr const char* kLegacyManifest = "cards.ini";
constexpr const char* kPartialSuffix = ".partial";
constexpr const char* kGaugeFolder = "grpdat\\Cockpit\\chara";
constexpr const char* kGaugeFile = "gauge_chr%03d.dds";
constexpr const char* kSelectFolder = "grpdat\\CSel";
constexpr const char* kSelectFile = "csel00.pat";
constexpr const char* kLanguageFile = "\\Save\\unist.ini";
constexpr const char* kLanguageSection = "System";
constexpr const char* kLanguageKey = "Language";
constexpr const char* kLanguagePrefix = "___";
constexpr int kFirstChara = 0;
constexpr int kLastChara = 27;
constexpr unsigned kMostWorkers = 4;
constexpr int kFullProgress = 100;
constexpr const char* kSection = "Portraits";
constexpr const char* kRevisionKey = "PaintRevision";
constexpr int kPaintRevision = 7;

struct Screen
{
	PortraitPainter::Screen screen;
	const char* folder;
	const char* file;
};

constexpr Screen kScreens[] = {
	{ PortraitPainter::Screen_Select, "grpdat\\CSel\\chara", "cs_chr%03d.pat" },
	{ PortraitPainter::Screen_Versus, "grpdat\\VsScreen", "vs_demo_chr%03d.pat" },
	{ PortraitPainter::Screen_Winner, "grpdat\\Winner", "win_ch_chr%03d.pat" },
	{ PortraitPainter::Screen_Menu, "grpdat\\MainMenuCS\\menucha", "menucha_chr%03d.pat" },
	{ PortraitPainter::Screen_Network, "grpdat\\Network\\new\\net_chr", "net_chr%03d.pat" },
};

BackgroundJob g_job("PortraitCompose");
std::mutex g_lock;
std::set<int> g_pending;
bool g_cardsPending = false;
volatile long g_queued = 0;
bool g_revisionChecked = false;
bool g_revisionPending = false;

std::string Numbered(const char* pattern, int chara)
{
	char text[64] = {};
	sprintf_s(text, pattern, chara);
	return text;
}

std::string Join(const std::string& folder, const std::string& name)
{
	return folder + "\\" + name;
}

std::string ScreenPath(const char* folder, const std::string& file)
{
	return Join(Join(PortraitCompose::Folder(), folder), file);
}

std::string CardPath(int chara)
{
	return Join(Join(PortraitLibrary::Root(), kCardsFolder), Numbered(kCardFile, chara));
}

bool WriteSafely(const std::string& path, const std::vector<uint8_t>& bytes)
{
	CreateDirectoryTree(path.substr(0, path.find_last_of('\\')));
	const std::string partial = path + kPartialSuffix;

	if (WriteWholeFile(partial, bytes) && MoveFileExA(partial.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
		return true;

	DeleteFileA(partial.c_str());
	return false;
}

void Status(int chara, const char* text)
{
	char status[160] = {};
	sprintf_s(status, "%s: %s", CharaTables::Name(chara), text);
	g_job.SetStatus(status);
}

void RemoveLegacy()
{
	const std::string root = PortraitLibrary::Root();
	RemoveDirectoryTree(Join(root, kLegacyScreens));

	if (GetFileAttributesA(Join(Join(root, kCardsFolder), kLegacyManifest).c_str()) != INVALID_FILE_ATTRIBUTES)
		RemoveDirectoryTree(Join(root, kCardsFolder));
}

void Forget(int chara)
{
	for (const Screen& screen : kScreens)
		DeleteFileA(ScreenPath(screen.folder, Numbered(screen.file, chara)).c_str());

	DeleteFileA(ScreenPath(kGaugeFolder, Numbered(kGaugeFile, chara)).c_str());
	DeleteFileA(CardPath(chara).c_str());
}

bool Fetched(int chara, const PortraitCatalog::Art& art)
{
	if (PortraitLibrary::IsReady(art))
		return true;

	std::string error;

	if (!PortraitDownload::Fetch(error))
	{
		Status(chara, ("the download failed: " + error).c_str());
		return false;
	}

	if (PortraitLibrary::IsReady(art))
		return true;

	Status(chara, "the portrait pack does not have that art");
	return false;
}

int PaintScreens(int chara, const PortraitPainter::Figure& figure)
{
	int written = 0;

	for (const Screen& screen : kScreens)
	{
		const std::string file = Numbered(screen.file, chara);
		std::vector<uint8_t> ours;
		std::vector<uint8_t> painted;

		if (!DataArchive::Read(screen.folder, file.c_str(), ours) ||
			!PortraitPainter::Repaint(ours, screen.screen, figure, painted))
		{
			LOG("PortraitCompose: %s was left as it is", file.c_str());
			continue;
		}

		written += WriteSafely(ScreenPath(screen.folder, file), painted) ? 1 : 0;
	}

	return written;
}

int PaintGauge(int chara, const PortraitPainter::Figure& figure)
{
	const std::string file = Numbered(kGaugeFile, chara);
	std::vector<uint8_t> bytes;
	DdsImage::Image ours;
	DdsImage::Image painted;

	if (!DataArchive::Read(kGaugeFolder, file.c_str(), bytes) || !DdsImage::Decode(bytes, ours) ||
		!PortraitPainter::RepaintGauge(ours, figure, painted))
	{
		LOG("PortraitCompose: %s was left as it is", file.c_str());
		return 0;
	}

	return WriteSafely(ScreenPath(kGaugeFolder, file), DdsImage::EncodeArgb(painted)) ? 1 : 0;
}

int PaintCard(int chara, const PortraitPainter::Figure& figure)
{
	return WriteSafely(CardPath(chara), DdsImage::EncodeArgb(PortraitPainter::PaintCard(figure))) ? 1 : 0;
}

bool Dress(int chara, const PortraitCatalog::Art& art)
{
	const PortraitFrames::Chara* const frames = PortraitFrames::Of(chara);
	DdsImage::Image image;

	if (frames == nullptr || !Fetched(chara, art))
		return false;

	if (!PortraitLibrary::Load(art, image))
	{
		Status(chara, "that art could not be read");
		return false;
	}

	const PortraitPainter::Figure figure = { &image, &art, frames };
	const int written = PaintScreens(chara, figure) + PaintGauge(chara, figure) + PaintCard(chara, figure);

	LOG("PortraitCompose: %s wears %s on %d file(s)", CharaTables::Name(chara), art.id, written);
	return written > 0;
}

void Recompose(int chara)
{
	const PortraitCatalog::Art* const art = PortraitCatalog::Find(PortraitChoices::Of(chara));
	Forget(chara);

	if (art == nullptr || art->chara != chara)
		return;

	if (!Dress(chara, *art))
		Forget(chara);
}

std::string LanguageFolder()
{
	char language[64] = {};
	GetPrivateProfileStringA(kLanguageSection, kLanguageKey, "", language, sizeof(language),
		(GetModDirectory() + kLanguageFile).c_str());

	return language[0] == '\0' ? std::string() : std::string(kLanguagePrefix) + language + "\\" + kSelectFolder;
}

bool ReadSelect(std::vector<uint8_t>& out)
{
	const std::string localised = LanguageFolder();

	if (!localised.empty() && DataArchive::Read(localised.c_str(), kSelectFile, out))
		return true;

	return DataArchive::Read(kSelectFolder, kSelectFile, out);
}

std::vector<PortraitPainter::Card> WornCards()
{
	std::vector<PortraitPainter::Card> cards;

	for (int chara = kFirstChara; chara <= kLastChara; ++chara)
	{
		std::vector<uint8_t> bytes;
		PortraitPainter::Card card = { chara, DdsImage::Image() };

		if (!PortraitChoices::Of(chara).empty() && ReadWholeFile(CardPath(chara), bytes) &&
			DdsImage::Decode(bytes, card.art))
		{
			cards.push_back(card);
		}
	}

	return cards;
}

void ComposeCards()
{
	const std::string path = ScreenPath(kSelectFolder, kSelectFile);
	const std::vector<PortraitPainter::Card> cards = WornCards();
	std::vector<uint8_t> select;
	std::vector<uint8_t> dressed;

	if (cards.empty() || !ReadSelect(select) || !PortraitPainter::DressCards(select, cards, dressed))
	{
		DeleteFileA(path.c_str());
		return;
	}

	WriteSafely(path, dressed);
}

unsigned WorkersFor(size_t charas)
{
	const unsigned cores = (std::max)(std::thread::hardware_concurrency(), 2u) - 1;
	return static_cast<unsigned>((std::min)({ static_cast<size_t>(cores), static_cast<size_t>(kMostWorkers), charas }));
}

void Painted(int done, int total)
{
	char status[96] = {};
	sprintf_s(status, "painting the screens (%d of %d)", done, total);
	g_job.SetStatus(status);
	g_job.SetProgress(done * kFullProgress / (total + 1));
}

void RecomposeAll(const std::set<int>& charas)
{
	const std::vector<int> queue(charas.begin(), charas.end());
	const int total = static_cast<int>(queue.size());
	std::atomic<size_t> next{ 0 };
	std::atomic<int> done{ 0 };
	std::vector<std::thread> workers;

	Painted(0, total);

	for (unsigned i = 0; i < WorkersFor(queue.size()); ++i)
	{
		workers.emplace_back([&]() {
			for (size_t at = next++; at < queue.size(); at = next++)
			{
				Recompose(queue[at]);
				Painted(++done, total);
			}
		});
	}

	for (std::thread& worker : workers)
		worker.join();
}

bool Run(const std::set<int>& charas)
{
	RemoveLegacy();
	RecomposeAll(charas);

	g_job.SetStatus("the character select grid");
	ComposeCards();
	g_job.SetProgress(kFullProgress);
	g_job.SetStatus("ready. Each screen shows the change the next time it opens");
	return true;
}

void StartPending()
{
	std::set<int> charas;

	{
		std::lock_guard<std::mutex> hold(g_lock);

		if (g_pending.empty() && !g_cardsPending)
		{
			InterlockedExchange(&g_queued, 0);
			return;
		}

		if (g_job.IsBusy())
			return;

		charas.swap(g_pending);
		g_cardsPending = false;
		InterlockedExchange(&g_queued, 0);
	}

	if (g_job.Start([charas]() { return Run(charas); }))
		return;

	std::lock_guard<std::mutex> hold(g_lock);
	g_pending.insert(charas.begin(), charas.end());
	g_cardsPending = true;
	InterlockedExchange(&g_queued, 1);
}

std::set<int> WornCharas()
{
	std::set<int> charas;

	for (int chara = kFirstChara; chara <= kLastChara; ++chara)
	{
		if (!PortraitChoices::Of(chara).empty())
			charas.insert(chara);
	}

	return charas;
}

void SaveRevision()
{
	Settings::SaveInt(kSection, kRevisionKey, kPaintRevision);
}

void RepaintOutdated()
{
	g_revisionChecked = true;

	if (static_cast<int>(GetPrivateProfileIntA(kSection, kRevisionKey, 0, Settings::GetIniPath().c_str())) == kPaintRevision)
		return;

	const std::set<int> worn = WornCharas();

	if (worn.empty())
	{
		SaveRevision();
		return;
	}

	LOG("PortraitCompose: repainting %d fighter(s) painted by an older revision", static_cast<int>(worn.size()));

	std::lock_guard<std::mutex> hold(g_lock);
	g_pending.insert(worn.begin(), worn.end());
	g_cardsPending = true;
	g_revisionPending = true;
	InterlockedExchange(&g_queued, 1);
}

void Finished()
{
	ModFiles::Rescan();

	if (!g_revisionPending)
		return;

	g_revisionPending = false;
	SaveRevision();
}

}

std::string PortraitCompose::Folder()
{
	return Join(PortraitLibrary::Root(), kScreensFolder);
}

void PortraitCompose::Wear(int chara, const std::string& id)
{
	if (PortraitChoices::Of(chara) == id)
		return;

	PortraitChoices::Choose(chara, id);

	{
		std::lock_guard<std::mutex> hold(g_lock);
		g_pending.insert(chara);
		InterlockedExchange(&g_queued, 1);
	}

	StartPending();
}

void PortraitCompose::Refresh()
{
	{
		std::lock_guard<std::mutex> hold(g_lock);
		g_cardsPending = true;
		InterlockedExchange(&g_queued, 1);
	}

	StartPending();
}

void PortraitCompose::Update()
{
	if (!g_revisionChecked)
		RepaintOutdated();

	if (g_job.ConsumeFinished())
		Finished();

	if (g_queued != 0)
		StartPending();
}

bool PortraitCompose::IsBusy()
{
	return g_job.IsBusy();
}

int PortraitCompose::Progress()
{
	return g_job.Progress();
}

std::string PortraitCompose::StatusText()
{
	return g_job.Status();
}
