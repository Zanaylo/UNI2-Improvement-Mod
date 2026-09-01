#include "Game/GamePatches.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Game/BalanceRules.h"
#include "Game/BgmControl.h"
#include "Game/DataSearchPath.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/OnlineState.h"
#include "Game/ReplayState.h"
#include "Game/ScriptReload.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr long kLoggedReads = 600;
constexpr long kLoggedMisses = 100;

SRWLOCK g_lock = SRWLOCK_INIT;
std::string g_prefix;
int g_active = -1;

int g_chosen = -1;
int g_replay = -1;
std::string g_bootId;
bool g_replayHeld = false;
bool g_sawPlayback = false;
bool g_loaded = false;
bool g_atNetworkMenu = false;
bool g_heldForOnline = false;

const char* g_reason = "your pick";
std::string g_announcement;
volatile long g_read = 0;
volatile long g_missing = 0;
char g_status[256] = "not loaded";

void Take(int index, const std::string& prefix)
{
	AcquireSRWLockExclusive(&g_lock);
	g_active = index;
	g_prefix = prefix;
	ReleaseSRWLockExclusive(&g_lock);
}

bool ApplyPatch(int index)
{
	const GamePatches::Patch* const patch = PatchLibrary::Get(index);

	if (patch == nullptr || !patch->present || patch->coverage.files == 0)
		return false;

	if (!DataSearchPath::Point(patch->prefix))
	{
		strncpy_s(g_status, "the game's data search path could not be written", _TRUNCATE);
		LOG("GamePatches: %s", g_status);
		return false;
	}

	std::string prefix = patch->prefix;

	if (!prefix.empty() && prefix.back() != '\\')
		prefix.push_back('\\');

	Take(index, prefix);
	BalanceRules::SetVersion(patch->version);

	sprintf_s(g_status, "reading %s - %d file(s), %d of %d characters", patch->name.c_str(),
		patch->coverage.files, patch->coverage.characters, patch->coverage.charactersWanted);

	LOG("GamePatches: %s", g_status);
	return true;
}

void ApplyInstalled()
{
	DataSearchPath::Release();
	BalanceRules::Release();
	Take(-1, std::string());

	strncpy_s(g_status, "reading the installed game", _TRUNCATE);
	LOG("GamePatches: %s", g_status);
}

bool Drifted(int index)
{
	const GamePatches::Patch* const patch = PatchLibrary::Get(index);

	if (patch == nullptr)
		return false;

	return !DataSearchPath::PointsAt(patch->prefix);
}

void Say(const std::string& text)
{
	AcquireSRWLockExclusive(&g_lock);
	g_announcement = text;
	ReleaseSRWLockExclusive(&g_lock);
}

void Announce()
{
	if (!g_replayHeld)
		return;

	const GamePatches::Patch* const patch = PatchLibrary::Get(g_active);

	Say(patch != nullptr
		? "Replay recorded on " + patch->name
		: std::string("Replay on the installed game"));
}

void GuardOnline()
{
	const bool here = BgmControl::GetLastRequested() == GameOffsets::kBgmNetworkMenu ||
		OnlineState::IsOnline() || OnlineState::HasSession();

	if (here == g_atNetworkMenu)
		return;

	g_atNetworkMenu = here;

	if (!here)
	{
		if (g_heldForOnline)
			Say("The patch stays unloaded until the game is restarted.");

		return;
	}

	const GamePatches::Patch* const patch = PatchLibrary::Get(g_active);

	if (patch == nullptr)
		return;

	if (!g_modVals.unloadPatchOnline)
	{
		Say(patch->name + " is loaded - anybody on the current game will desync. "
			"Reload into the installed game before a ranked or player match.");

		LOG("GamePatches: the network menu opened while %s is loaded", patch->name.c_str());
		return;
	}

	const std::string name = patch->name;

	g_heldForOnline = true;
	g_reason = "unloaded for online";
	ApplyInstalled();

	Say(name + " was unloaded - the game reads the installed build from here on. The tables it "
		"already read at startup are still the patch's, so restart before a ranked or player "
		"match.");

	LOG("GamePatches: %s was unloaded because the game went online", name.c_str());
}

void RebuildTables()
{
	if (!PatchLibrary::ReloadsTables() || !ScriptReload::Run())
		return;

	const GamePatches::Patch* const patch = PatchLibrary::Get(g_active);

	g_bootId = patch != nullptr ? patch->id : std::string();
}

void ApplyIndex(int index)
{
	if (g_heldForOnline)
		index = -1;

	if (index == g_active && !Drifted(index))
		return;

	const bool changed = index != g_active;

	if (changed)
	{
		InterlockedExchange(&g_read, 0);
		InterlockedExchange(&g_missing, 0);
	}

	if (index < 0 || !ApplyPatch(index))
		ApplyInstalled();

	if (changed)
	{
		RebuildTables();
		Announce();
	}
}

int Desired()
{
	g_reason = "picked at startup";
	return PatchLibrary::IndexOfId(g_bootId.c_str());
}

void ReleaseReplayHold()
{
	if (!g_replayHeld)
		return;

	if (ReplayState::IsPlaying())
	{
		g_sawPlayback = true;
		return;
	}

	if (g_sawPlayback)
		g_replayHeld = false;
}

void Remember()
{
	const GamePatches::Patch* const chosen = PatchLibrary::Get(g_chosen);

	PatchLibrary::RememberActive(chosen != nullptr ? chosen->id.c_str() : "");
}

}

void GamePatches::Load()
{
	PatchLibrary::Load();
	g_loaded = true;
}

int GamePatches::Count()
{
	if (!g_loaded)
		Load();

	return PatchLibrary::Count();
}

const GamePatches::Patch* GamePatches::Get(int index)
{
	if (!g_loaded)
		Load();

	return PatchLibrary::Get(index);
}

int GamePatches::ActiveIndex()
{
	return g_active;
}

int GamePatches::ChosenIndex()
{
	return g_chosen;
}

int GamePatches::BootIndex()
{
	return PatchLibrary::IndexOfId(g_bootId.c_str());
}

int GamePatches::ReplayWanted()
{
	return g_replayHeld ? g_replay : -1;
}

bool GamePatches::TablesAgreeWith(int index)
{
	return index == BootIndex();
}

void GamePatches::ApplyForReset(int index)
{
	const Patch* const patch = PatchLibrary::Get(index);

	g_heldForOnline = false;
	g_chosen = patch != nullptr ? index : -1;
	g_bootId = patch != nullptr ? patch->id : std::string();
	g_replayHeld = false;
	g_sawPlayback = false;

	PatchLibrary::RememberActive(patch != nullptr ? patch->id.c_str() : "");

	ApplyIndex(g_chosen);

	LOG("GamePatches: %s is in place, the game is about to read it from the start",
		patch != nullptr ? patch->name.c_str() : "the installed game");
}

GamePatches::Answer GamePatches::Resolve(const char* path, std::string& out)
{
	if (path == nullptr || path[0] == 0 || g_active < 0)
		return Answer_NotOurs;

	AcquireSRWLockShared(&g_lock);

	const size_t prefix = g_prefix.size();
	const bool ours = prefix != 0 && _strnicmp(path, g_prefix.c_str(), prefix) == 0;
	const std::string* found = nullptr;
	std::string olderName;

	if (ours)
	{
		const std::string key = FileIndex::Key(path + prefix, strlen(path + prefix));

		for (int i = g_active; i >= 0 && found == nullptr; --i)
		{
			const Patch* const patch = PatchLibrary::Get(i);

			if (patch == nullptr)
				continue;

			found = patch->files.Find(key);

			if (found != nullptr && i != g_active)
				olderName = patch->name;
		}
	}

	if (found != nullptr)
		out = *found;

	ReleaseSRWLockShared(&g_lock);

	if (!ours)
		return Answer_NotOurs;

	if (found == nullptr)
	{
		if (InterlockedIncrement(&g_missing) <= kLoggedMisses)
			LOG("GamePatches: %s is in no patch, the installed game answers it", path + prefix);

		return Answer_Missing;
	}

	if (InterlockedIncrement(&g_read) <= kLoggedReads)
	{
		if (olderName.empty())
			LOG("GamePatches: %s -> %s", path + prefix, out.c_str());
		else
			LOG("GamePatches: %s -> %s, carried back from %s", path + prefix, out.c_str(),
				olderName.c_str());
	}

	return Answer_Found;
}

int GamePatches::FilesRead()
{
	return static_cast<int>(g_read);
}

int GamePatches::FilesMissing()
{
	return static_cast<int>(g_missing);
}

const char* GamePatches::WhyActive()
{
	return g_reason;
}

bool GamePatches::UnloadedForOnline()
{
	return g_heldForOnline;
}

bool GamePatches::TakeAnnouncement(std::string& out)
{
	AcquireSRWLockExclusive(&g_lock);

	const bool pending = !g_announcement.empty();

	if (pending)
	{
		out = g_announcement;
		g_announcement.clear();
	}

	ReleaseSRWLockExclusive(&g_lock);
	return pending;
}

bool GamePatches::IsAuto()
{
	return PatchLibrary::Automatic();
}

void GamePatches::SetAuto(bool enabled)
{
	PatchLibrary::SetAutomatic(enabled);
}

bool GamePatches::IsSupported()
{
	return DataSearchPath::IsSupported();
}

bool GamePatches::RebuildsTables()
{
	return PatchLibrary::ReloadsTables();
}

void GamePatches::SetRebuildsTables(bool enabled)
{
	PatchLibrary::SetReloadsTables(enabled);
}

bool GamePatches::CanRebuildTables()
{
	return ScriptReload::IsSupported();
}

const char* GamePatches::RebuildStatus()
{
	return ScriptReload::StatusText();
}

void GamePatches::Update()
{
	if (!g_loaded)
		return;

	ReleaseReplayHold();
	GuardOnline();
	ApplyIndex(Desired());
}

void GamePatches::Choose(int index)
{
	g_chosen = index >= 0 && index < Count() ? index : -1;
	g_replayHeld = false;
	g_sawPlayback = false;

	Remember();
	Announce();
}

void GamePatches::OnReplayStarting(const SYSTEMTIME& when)
{
	if (!IsAuto())
		return;

	const int wanted = ForDate(when);

	if (wanted < 0)
	{
		LOG("GamePatches: no patch covers a replay from %04d-%02d-%02d",
			when.wYear, when.wMonth, when.wDay);
		return;
	}

	g_replay = wanted;
	g_replayHeld = true;
	g_sawPlayback = false;

	if (wanted == BootIndex())
		return;

	const Patch* const patch = PatchLibrary::Get(wanted);

	LOG("GamePatches: this replay was recorded on %s, and the game started on something else",
		patch != nullptr ? patch->name.c_str() : "another build");
}

void GamePatches::ApplyRemembered()
{
	Load();

	const std::string remembered = PatchLibrary::RememberedActive();

	g_chosen = PatchLibrary::IndexOfId(remembered.c_str());
	g_bootId = g_chosen >= 0 ? remembered : std::string();

	if (!remembered.empty() && g_chosen < 0)
		LOG("GamePatches: the remembered patch '%s' is not on the list any more", remembered.c_str());

	if (!remembered.empty())
	{
		PatchLibrary::RememberActive("");
		LOG("GamePatches: the pick is used up - the next plain launch is the installed game");
	}

	ApplyIndex(g_chosen);
}

int GamePatches::ForDate(const SYSTEMTIME& when)
{
	if (!g_loaded)
		Load();

	return PatchLibrary::Newest(when);
}

bool GamePatches::Import(const std::string& folder, const std::string& name, char* status,
	int statusSize)
{
	if (!g_loaded)
		Load();

	if (!PatchLibrary::Add(folder, name, status, statusSize))
		return false;

	g_chosen = PatchLibrary::IndexOfId(PatchLibrary::RememberedActive().c_str());
	ApplyIndex(Desired());
	return true;
}

bool GamePatches::AdoptImport(std::unique_ptr<PatchLibrary::Patch> patch,
	const std::string& note, const SYSTEMTIME& released, char* status, int statusSize)
{
	if (!g_loaded)
		Load();

	return PatchLibrary::Adopt(std::move(patch), note, released, status, statusSize);
}

void GamePatches::FinishImport()
{
	PatchLibrary::Save();

	g_chosen = PatchLibrary::IndexOfId(PatchLibrary::RememberedActive().c_str());
	ApplyIndex(Desired());
}

void GamePatches::SetDate(int index, const SYSTEMTIME& released)
{
	const Patch* const chosen = PatchLibrary::Get(g_chosen);
	const std::string id = chosen != nullptr ? chosen->id : std::string();

	PatchLibrary::SetDate(index, released);

	g_chosen = PatchLibrary::IndexOfId(id.c_str());
	ApplyIndex(Desired());
}

void GamePatches::Describe(int index, const std::string& note, const SYSTEMTIME& released)
{
	const Patch* const chosen = PatchLibrary::Get(g_chosen);
	const std::string id = chosen != nullptr ? chosen->id : std::string();

	PatchLibrary::Describe(index, note, released);

	g_chosen = PatchLibrary::IndexOfId(id.c_str());
	ApplyIndex(Desired());
}

bool GamePatches::Forget(int index)
{
	ApplyInstalled();

	const Patch* const chosen = PatchLibrary::Get(g_chosen);
	const std::string id = index == g_chosen || chosen == nullptr ? std::string() : chosen->id;

	if (!PatchLibrary::Remove(index))
		return false;

	g_chosen = PatchLibrary::IndexOfId(id.c_str());
	g_replayHeld = false;

	Remember();
	ApplyIndex(Desired());
	return true;
}

std::string GamePatches::Root()
{
	return PatchLibrary::Root();
}

const char* GamePatches::StatusText()
{
	return g_status;
}
