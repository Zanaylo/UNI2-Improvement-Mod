#include "Game/Stages/StageLibrary.h"

#include "Core/Config/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/BgCeiling.h"
#include "Game/Stages/BgListOverride.h"
#include "Game/Stages/ExtraStages.h"
#include "Game/Files/FbGameFolder.h"
#include "Game/Stages/StageArchive.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kSection = "StageLibrary";
constexpr const char* kLegacySection = "Stages";
constexpr const char* kNote = "stage.txt";
constexpr const char* kModel = "bg.fbx.bin";
constexpr uint64_t kFnvOffset = 0xcbf29ce484222325ull;
constexpr uint64_t kFnvPrime = 0x100000001b3ull;
constexpr int kOwnEntries = 30;
constexpr size_t kFirstSectionBytes = 8192;
constexpr size_t kLastSectionBytes = 512 * 1024;

std::vector<StageLibrary::Entry> g_entries;
std::vector<int> g_slots;
int g_bySlot[StageLibrary::kSlotLast + 1] = {};
SRWLOCK g_lock = SRWLOCK_INIT;
volatile long g_revision = 0;

bool g_ownedKnown = false;
bool g_slotsFromOwned = false;
bool g_owned[StageLibrary::kSlotLast + 1] = {};
int g_ownEntries = kOwnEntries;

void LearnOwned()
{
	if (g_ownedKnown)
		return;

	std::vector<int> numbers;

	if (!BgListOverride::OwnNumbers(numbers))
		return;

	int highest = -1;

	for (int number : numbers)
	{
		if (number < 0 || number > StageLibrary::kSlotLast)
			continue;

		g_owned[number] = true;
		highest = number > highest ? number : highest;
	}

	g_ownEntries = static_cast<int>(numbers.size());
	g_ownedKnown = true;

	LOG("StageLibrary: the game's own list claims %d stage(s), up to bg%03d", g_ownEntries,
		highest);
}

const std::vector<int>& Slots()
{
	if (!g_slots.empty() && g_slotsFromOwned == g_ownedKnown)
		return g_slots;

	g_slots.clear();

	for (int slot = StageLibrary::kSlotFirst; slot <= StageLibrary::kSlotLast; ++slot)
	{
		if (StageLibrary::Bindable(slot))
			g_slots.push_back(slot);
	}

	g_slotsFromOwned = g_ownedKnown;

	return g_slots;
}

void Assign()
{
	const std::vector<int>& slots = Slots();
	const int budget = StageLibrary::SlotBudget();
	int position = 0;

	for (StageLibrary::Entry& entry : g_entries)
	{
		entry.position = entry.shown ? position++ : -1;
		entry.slot = entry.position >= 0 && entry.position < budget
			? slots[entry.position] : -1;
	}

	int bySlot[StageLibrary::kSlotLast + 1] = {};

	for (const StageLibrary::Entry& entry : g_entries)
	{
		if (StageLibrary::Bindable(entry.slot))
			bySlot[entry.slot] = entry.id + 1;
	}

	if (memcmp(bySlot, g_bySlot, sizeof(bySlot)) == 0)
		return;

	memcpy(g_bySlot, bySlot, sizeof(bySlot));
	InterlockedIncrement(&g_revision);
}

std::string Key(int id)
{
	char key[16] = {};
	sprintf_s(key, "Lib%d", id);

	return key;
}

bool Section(const char* section, std::vector<std::string>& out)
{
	out.clear();

	std::vector<char> buffer(kFirstSectionBytes, 0);

	for (;;)
	{
		const DWORD read = GetPrivateProfileSectionA(section, buffer.data(),
			static_cast<DWORD>(buffer.size()), Settings::GetIniPath().c_str());

		if (read < buffer.size() - 2)
			break;

		if (buffer.size() >= kLastSectionBytes)
			return false;

		buffer.assign(buffer.size() * 2, 0);
	}

	for (const char* at = buffer.data(); *at != 0; at += strlen(at) + 1)
		out.push_back(at);

	return true;
}

bool Split(const std::string& line, std::string& key, std::string& value)
{
	const size_t equals = line.find('=');

	if (equals == std::string::npos)
		return false;

	key = line.substr(0, equals);
	value = line.substr(equals + 1);

	return !key.empty();
}

int NumberAfter(const std::string& text, const char* prefix)
{
	const size_t length = strlen(prefix);

	if (text.compare(0, length, prefix) != 0 || text.size() <= length)
		return -1;

	for (size_t at = length; at < text.size(); ++at)
	{
		if (isdigit(static_cast<unsigned char>(text[at])) == 0)
			return -1;
	}

	return atoi(text.c_str() + length);
}

void Field(const std::string& text, size_t& at, std::string& out)
{
	const size_t bar = text.find('|', at);

	out = bar == std::string::npos ? text.substr(at) : text.substr(at, bar - at);
	at = bar == std::string::npos ? text.size() : bar + 1;
}

std::string Lowered(const std::string& text)
{
	std::string out = text;

	for (char& character : out)
		character = static_cast<char>(tolower(static_cast<unsigned char>(character)));

	return out;
}

std::string Hashed(const std::string& text)
{
	uint64_t hash = kFnvOffset;

	for (const char character : text)
	{
		hash ^= static_cast<uint8_t>(character);
		hash *= kFnvPrime;
	}

	char out[20] = {};
	sprintf_s(out, "%016llx", static_cast<unsigned long long>(hash));

	return out;
}

std::string Identity(const StageLibrary::Entry& entry)
{
	if (entry.game.empty() && entry.folder.empty())
		return Lowered(entry.name);

	return Lowered(entry.game) + "|" + Lowered(entry.folder);
}

void AssignKeys()
{
	std::vector<std::string> taken;

	for (StageLibrary::Entry& entry : g_entries)
	{
		entry.key = Hashed(Identity(entry));

		if (std::find(taken.begin(), taken.end(), entry.key) != taken.end())
			entry.key = Hashed(Identity(entry) + "#" + std::to_string(entry.id));

		taken.push_back(entry.key);
	}
}

bool Insert(const StageLibrary::Entry& entry)
{
	for (StageLibrary::Entry& known : g_entries)
	{
		if (known.id != entry.id)
			continue;

		known = entry;
		return false;
	}

	g_entries.push_back(entry);
	return true;
}

bool Known(int id)
{
	for (const StageLibrary::Entry& entry : g_entries)
	{
		if (entry.id == id)
			return true;
	}

	return false;
}

void Save(const StageLibrary::Entry& entry)
{
	const std::string value = std::string(entry.shown ? "1" : "0") + "|" + entry.game + "|"
		+ entry.folder + "|" + entry.name;

	Settings::SaveString(kSection, Key(entry.id).c_str(), value.c_str());
}

bool Showing(const std::string& first)
{
	return first != "0" && first != "-1";
}

std::string Unquoted(const std::string& note, const char* key)
{
	std::string value;

	return StageArchive::Field(note, key, value) ? StageArchive::Unquoted(value) : std::string();
}

bool NoteText(int id, std::string& out)
{
	std::vector<uint8_t> blob;

	if (!ReadWholeFile(StageLibrary::NoteOf(id), blob) || blob.empty())
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
}

void ReadNote(StageLibrary::Entry& entry)
{
	std::string note;

	if (!NoteText(entry.id, note))
		return;

	const std::string name = Unquoted(note, "Name");
	const std::string from = Unquoted(note, "From");
	const std::string source = Unquoted(note, "Source");

	if (!name.empty())
		entry.name = name;

	if (!source.empty())
		entry.folder = source;

	if (from.empty())
		return;

	const FbGameFolder::Game game = FbGameFolder::Detect(from.c_str());

	entry.game = game == FbGameFolder::Game_None ? from : FbGameFolder::Name(game);
}

void ReadSection()
{
	std::vector<std::string> lines;

	if (!Section(kSection, lines))
		return;

	for (const std::string& line : lines)
	{
		std::string key;
		std::string value;

		if (!Split(line, key, value))
			continue;

		const int id = NumberAfter(key, "Lib");

		if (id < StageLibrary::kSlotFirst || id > StageLibrary::kIdLast)
			continue;

		StageLibrary::Entry entry = {};
		entry.id = id;

		size_t at = 0;
		std::string shown;

		Field(value, at, shown);
		Field(value, at, entry.game);
		Field(value, at, entry.folder);
		Field(value, at, entry.name);

		entry.shown = Showing(shown);
		entry.slot = -1;

		Insert(entry);
	}
}

void Migrate()
{
	std::vector<std::string> lines;

	if (!Section(kLegacySection, lines))
		return;

	for (const std::string& line : lines)
	{
		std::string key;
		std::string value;

		if (!Split(line, key, value))
			continue;

		const int slot = NumberAfter(key, "Stage");

		if (slot < StageLibrary::kSlotFirst || slot > StageLibrary::kSlotLast || value.empty())
			continue;

		StageLibrary::Entry entry = {};
		entry.id = slot;
		entry.slot = -1;
		entry.shown = true;

		size_t at = 0;

		Field(value, at, entry.game);
		Field(value, at, entry.folder);
		Field(value, at, entry.name);

		Insert(entry);
		Save(entry);

		LOG("StageLibrary: stage %d carried over from the old [Stages] list", entry.id);
	}

	for (const std::string& line : lines)
	{
		std::string key;
		std::string value;

		if (Split(line, key, value))
			Settings::SaveString(kLegacySection, key.c_str(), nullptr);
	}
}

bool OnDisk(int id)
{
	const DWORD attributes = GetFileAttributesA(StageLibrary::FolderOf(id).c_str());

	return attributes != INVALID_FILE_ATTRIBUTES
		&& (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void Prune()
{
	std::vector<StageLibrary::Entry> kept;

	for (const StageLibrary::Entry& entry : g_entries)
	{
		if (OnDisk(entry.id))
		{
			kept.push_back(entry);
			continue;
		}

		Settings::SaveString(kSection, Key(entry.id).c_str(), nullptr);

		LOG("StageLibrary: bg%03d '%s' is no longer on disk and left the list", entry.id,
			entry.name.c_str());
	}

	g_entries.swap(kept);
}

int FirstFree(const std::vector<int>& reserved)
{
	for (int id = StageLibrary::kSlotFirst; id <= StageLibrary::kIdLast; ++id)
	{
		if (StageLibrary::GameOwns(id) || Known(id))
			continue;

		if (std::find(reserved.begin(), reserved.end(), id) != reserved.end())
			continue;

		if (GetFileAttributesA(StageLibrary::FolderOf(id).c_str()) != INVALID_FILE_ATTRIBUTES)
			continue;

		return id;
	}

	return -1;
}

bool IsStageFolder(const std::string& folder)
{
	return GetFileAttributesA((folder + "\\" + kModel).c_str()) != INVALID_FILE_ATTRIBUTES;
}

void NameInNote(const std::string& folder, const std::string& name)
{
	const std::string path = folder + "\\" + kNote;

	if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
		return;

	FILE* file = nullptr;

	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
		return;

	fprintf(file, "// UNI2 Improvement Mod\r\nName = \"%s\"\r\nSource = \"%s\"\r\n", name.c_str(),
		name.c_str());
	fclose(file);
}

void AdoptLoose(const std::string& leaf)
{
	const std::string from = StageLibrary::Root() + "\\" + leaf;

	if (!IsStageFolder(from))
		return;

	const int id = FirstFree(std::vector<int>());

	if (id < 0)
	{
		LOG("StageLibrary: '%s' is a stage but every folder number is taken", leaf.c_str());
		return;
	}

	if (!MoveFileA(from.c_str(), StageLibrary::FolderOf(id).c_str()))
	{
		LOG("StageLibrary: '%s' could not become bg%03d (error %lu)", leaf.c_str(), id,
			GetLastError());
		return;
	}

	NameInNote(StageLibrary::FolderOf(id), leaf);

	StageLibrary::Entry entry = {};
	entry.id = id;
	entry.slot = -1;
	entry.shown = true;
	entry.name = leaf;

	ReadNote(entry);
	Insert(entry);
	Save(entry);

	LOG("StageLibrary: the folder '%s' was dropped in with no number, it is bg%03d now", leaf.c_str(),
		id);
}

void Adopt()
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((StageLibrary::Root() + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || found.cFileName[0] == '.')
			continue;

		const int id = NumberAfter(found.cFileName, "bg");

		if (id < 0)
		{
			AdoptLoose(found.cFileName);
			continue;
		}

		if (id < StageLibrary::kSlotFirst || id > StageLibrary::kIdLast
			|| StageLibrary::GameOwns(id) || Known(id))
		{
			continue;
		}

		StageLibrary::Entry entry = {};
		entry.id = id;
		entry.slot = -1;
		entry.shown = true;
		entry.name = found.cFileName;

		ReadNote(entry);
		Insert(entry);
		Save(entry);

		LOG("StageLibrary: adopted bg%03d, '%s', which no list mentioned", entry.id,
			entry.name.c_str());
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

void Rename()
{
	for (StageLibrary::Entry& entry : g_entries)
	{
		std::string note;

		if (!NoteText(entry.id, note))
			continue;

		const std::string name = Unquoted(note, "Name");

		if (name.empty() || name == entry.name)
			continue;

		LOG("StageLibrary: bg%03d is '%s' now, as its stage.txt says, not '%s'", entry.id,
			name.c_str(), entry.name.c_str());

		entry.name = name;
		Save(entry);
	}
}

}

void StageLibrary::Load()
{
	AcquireSRWLockExclusive(&g_lock);

	g_entries.clear();
	ReadSection();

	if (g_entries.empty())
		Migrate();

	Prune();
	Adopt();
	Rename();

	std::sort(g_entries.begin(), g_entries.end(),
		[](const Entry& a, const Entry& b) { return a.id < b.id; });

	AssignKeys();
	Assign();

	ReleaseSRWLockExclusive(&g_lock);
}

bool StageLibrary::GameOwns(int number)
{
	LearnOwned();

	if (number < kSlotFirst || number == kTrainingStage || number == kDebugStage)
		return true;

	return number >= 0 && number <= kSlotLast && g_owned[number];
}

bool StageLibrary::Bindable(int slot)
{
	return slot >= kSlotFirst && slot <= kSlotLast && slot < BgCeiling::Numbers() &&
		!GameOwns(slot);
}

int StageLibrary::SlotBudget()
{
	LearnOwned();

	const int slots = static_cast<int>(Slots().size());
	const int room = BgCeiling::ListEntries() - g_ownEntries;

	return slots < room ? slots : room;
}

int StageLibrary::Room()
{
	const int room = SlotBudget() - ShownCount();

	return room < 0 ? 0 : room;
}

int StageLibrary::SlotAt(int index)
{
	const std::vector<int>& slots = Slots();

	return index < 0 || index >= static_cast<int>(slots.size()) ? -1 : slots[index];
}

int StageLibrary::ShownCount()
{
	AcquireSRWLockShared(&g_lock);

	int shown = 0;

	for (const Entry& entry : g_entries)
		shown += entry.shown ? 1 : 0;

	ReleaseSRWLockShared(&g_lock);

	return shown;
}

void StageLibrary::Snapshot(std::vector<Entry>& out)
{
	AcquireSRWLockShared(&g_lock);
	out = g_entries;
	ReleaseSRWLockShared(&g_lock);
}

int StageLibrary::Total()
{
	int own = 0;

	for (int i = 0; i < ExtraStages::StageCount(); ++i)
	{
		const ExtraStages::Stage* const stage = ExtraStages::StageAt(i);

		own += stage != nullptr && GameOwns(stage->number) ? 1 : 0;
	}

	return own + Count();
}

int StageLibrary::Count()
{
	AcquireSRWLockShared(&g_lock);
	const int count = static_cast<int>(g_entries.size());
	ReleaseSRWLockShared(&g_lock);

	return count;
}

bool StageLibrary::Of(int id, Entry& out)
{
	AcquireSRWLockShared(&g_lock);

	bool found = false;

	for (const Entry& entry : g_entries)
	{
		if (entry.id != id)
			continue;

		out = entry;
		found = true;
		break;
	}

	ReleaseSRWLockShared(&g_lock);

	return found;
}

int StageLibrary::IdForSlot(int slot)
{
	if (!Bindable(slot))
		return -1;

	AcquireSRWLockShared(&g_lock);
	const int held = g_bySlot[slot];
	ReleaseSRWLockShared(&g_lock);

	return held == 0 ? -1 : held - 1;
}

int StageLibrary::SlotOf(int id)
{
	Entry entry = {};

	return Of(id, entry) ? entry.slot : -1;
}

int StageLibrary::FreeId(const std::vector<int>& reserved)
{
	AcquireSRWLockShared(&g_lock);
	const int free = FirstFree(reserved);
	ReleaseSRWLockShared(&g_lock);

	return free;
}

long StageLibrary::Revision()
{
	return InterlockedCompareExchange(&g_revision, 0, 0);
}

std::string StageLibrary::KeyOf(int id)
{
	Entry entry = {};

	return Of(id, entry) ? entry.key : std::string();
}

void StageLibrary::Put(const Entry& entry)
{
	AcquireSRWLockExclusive(&g_lock);

	Insert(entry);
	Save(entry);

	std::sort(g_entries.begin(), g_entries.end(),
		[](const Entry& a, const Entry& b) { return a.id < b.id; });

	AssignKeys();
	Assign();

	ReleaseSRWLockExclusive(&g_lock);
}

void StageLibrary::Show(int id, bool shown)
{
	AcquireSRWLockExclusive(&g_lock);

	for (Entry& entry : g_entries)
	{
		if (entry.id != id || entry.shown == shown)
			continue;

		entry.shown = shown;
		Save(entry);
		Assign();
		break;
	}

	ReleaseSRWLockExclusive(&g_lock);
}

void StageLibrary::Erase(int id)
{
	AcquireSRWLockExclusive(&g_lock);

	g_entries.erase(std::remove_if(g_entries.begin(), g_entries.end(),
		[id](const Entry& entry) { return entry.id == id; }), g_entries.end());

	Settings::SaveString(kSection, Key(id).c_str(), nullptr);
	Assign();

	ReleaseSRWLockExclusive(&g_lock);
}

std::string StageLibrary::Root()
{
	return GetModRootPath("Mods\\bg");
}

std::string StageLibrary::FolderOf(int id)
{
	char leaf[16] = {};
	sprintf_s(leaf, "\\bg%03d", id);

	return Root() + leaf;
}

std::string StageLibrary::NoteOf(int id)
{
	return FolderOf(id) + "\\" + kNote;
}
