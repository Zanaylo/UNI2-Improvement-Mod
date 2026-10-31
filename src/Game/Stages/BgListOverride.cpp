#include "Game/Stages/BgListOverride.h"

#include "Core/Config/Features.h"
#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/FileIndex.h"
#include "Core/Formats/Deflate.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Files/DataArchive.h"
#include "Game/Files/ModPacks.h"
#include "Game/Stages/BgTable.h"
#include "Game/Stages/PickerOrder.h"
#include "Game/Stages/StageNote.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr const char* kList = "BgList.txt";
constexpr const char* kNames = "BgList_str.txt";
constexpr const char* kServedList = "bg\\BgList.txt";
constexpr const char* kServedNames = "bg\\BgList_str.txt";
constexpr const char* kUserFolder = "Mods\\bg\\";
constexpr const char* kPackFolder = "\\bg\\";
constexpr const char* kComposedFolder = "Generated";
constexpr const char* kRetiredSuffix = ".old";
constexpr const char* kSection = "StagePicker";
constexpr const char* kOrderKey = "Order";
constexpr const char* kHiddenKey = "Hidden";
constexpr const char* kAddedKey = "Added";
constexpr const char* kLayeredKey = "Layered";
constexpr const char* kLearnKey = "LearnFromOrder";
constexpr const char* kFollowedListKey = "ModsListStamp";
constexpr const char* kOn = "1";
constexpr DWORD kStoredBytes = 16384;

struct TableLayer
{
	std::string folder;
	bool yours;
	BgTable::Text text;
};

struct Inputs
{
	std::vector<BgListOverride::Slotted> ours;
	std::vector<int> owned;
	std::vector<std::string> ourFiles;
	std::vector<BgListOverride::Reworked> reworked;
};

SRWLOCK g_lock = SRWLOCK_INIT;
BgTable::Text g_composed;
std::vector<int> g_standard;
Inputs g_inputs;
bool g_synced = false;
bool g_committed = false;
bool g_served = false;
bool g_restart = false;

bool Contains(const std::vector<int>& numbers, int number)
{
	return std::find(numbers.begin(), numbers.end(), number) != numbers.end();
}

bool ContainsText(const std::vector<std::string>& texts, const std::string& text)
{
	return std::find(texts.begin(), texts.end(), text) != texts.end();
}

std::string UserPath(const char* leaf)
{
	return GetModRootPath(std::string(kUserFolder) + leaf);
}

std::string ComposedPath(const char* leaf)
{
	return GetModRootPath(std::string(kComposedFolder) + "\\" + leaf);
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool ReadText(const std::string& path, std::string& out)
{
	std::vector<uint8_t> blob;

	if (!ReadWholeFile(path, blob) || blob.empty())
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
}

bool WriteText(const std::string& path, const std::string& text)
{
	return WriteWholeFile(path, std::vector<uint8_t>(text.begin(), text.end()));
}

bool Original(const char* leaf, std::string& out)
{
	std::vector<uint8_t> blob;

	if (!DataArchive::Read("bg", leaf, blob) || blob.empty())
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
}

std::string Stored(const char* key)
{
	std::vector<char> value(kStoredBytes, 0);
	Ini::GetString(kSection, key, "", value.data(), kStoredBytes, Settings::GetIniPath().c_str());

	return value.data();
}

void Store(const char* key, const std::string& value)
{
	Settings::SaveString(kSection, key, value.c_str());
}

std::string NameKey(int number)
{
	char key[16] = {};
	sprintf_s(key, "Name%03d", number);

	return key;
}

PickerOrder::Choices LoadChoices()
{
	PickerOrder::Choices choices;
	choices.order = PickerOrder::Split(Stored(kOrderKey));
	choices.hidden = PickerOrder::Split(Stored(kHiddenKey));
	choices.added = PickerOrder::Split(Stored(kAddedKey));

	return choices;
}

void SaveChoices(const PickerOrder::Choices& choices)
{
	Store(kOrderKey, PickerOrder::Join(choices.order));
	Store(kHiddenKey, PickerOrder::Join(choices.hidden));
	Store(kAddedKey, PickerOrder::Join(choices.added));
}

bool Layered()
{
	return Stored(kLayeredKey) == kOn;
}

bool ReadLayer(const std::string& folder, BgTable::Text& layer)
{
	const bool list = ReadText(folder + kList, layer.list);
	const bool names = ReadText(folder + kNames, layer.names);

	return list || names;
}

std::vector<TableLayer> Sources()
{
	std::vector<TableLayer> sources;

	if (Features::IsLoaded(FeatureCatalog::Id_ModPacks))
	{
		for (const std::string& folder : ModPacks::EnabledFolders())
			sources.push_back({ folder + kPackFolder, false, {} });
	}

	if (Layered())
		sources.push_back({ GetModRootPath(kUserFolder), true, {} });

	return sources;
}

std::vector<TableLayer> Layers(const std::vector<std::string>& ourFiles)
{
	std::vector<TableLayer> layers;

	for (TableLayer& layer : Sources())
	{
		if (!ReadLayer(layer.folder, layer.text))
		{
			LOG("BgListOverride: %s has no %s or %s", layer.folder.c_str(), kList, kNames);
			continue;
		}

		std::vector<int> wanted;
		const bool lists = BgTable::Listed(layer.text.list, wanted);

		LOG("BgListOverride: %s holds %d stage(s), %d name(s) and %s", layer.folder.c_str(),
			static_cast<int>(BgTable::Numbers(layer.text.list).size()),
			static_cast<int>(BgTable::Named(layer.text.names).size()), lists ? "a BgSelectList" : "no BgSelectList");

		const std::vector<int> dropped = BgTable::DropKeyed(layer.text, ourFiles);

		if (!dropped.empty())
		{
			LOG("BgListOverride: %d stage(s) in %s%s are copies of the mod's stages, the mod places those itself",
				static_cast<int>(dropped.size()), layer.folder.c_str(), kList);
		}

		layers.push_back(layer);
	}

	return layers;
}

std::vector<BgTable::Text> Texts(const std::vector<TableLayer>& layers)
{
	std::vector<BgTable::Text> texts;

	for (const TableLayer& layer : layers)
		texts.push_back(layer.text);

	return texts;
}

std::vector<PickerOrder::Keyed> Keyed(const std::string& list, const std::vector<int>& numbers)
{
	std::vector<PickerOrder::Keyed> keyed;

	for (int number : numbers)
		keyed.push_back({ number, BgTable::Key(list, number) });

	return keyed;
}

bool Retire(const std::string& path)
{
	if (!Exists(path))
		return true;

	return MoveFileExA(path.c_str(), (path + kRetiredSuffix).c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

std::vector<int> Foreign(const std::string& list, const std::vector<int>& gameNumbers, const std::vector<int>& owned,
	const std::vector<std::string>& ourFiles)
{
	std::vector<int> foreign;

	for (int number : BgTable::Numbers(list))
	{
		if (Contains(gameNumbers, number) || Contains(owned, number) || ContainsText(ourFiles, BgTable::Key(list, number)))
			continue;

		foreign.push_back(number);
	}

	return foreign;
}

void Rework(BgTable::Text& composed, const std::vector<BgListOverride::Reworked>& reworked)
{
	for (const BgListOverride::Reworked& one : reworked)
	{
		std::string entry;

		if (!BgTable::Entry(composed.list, one.number, entry))
			continue;

		BgTable::Put(composed.list, one.number, StageNote::Rework(entry, one.note));

		if (!one.shiftJisName.empty())
			BgTable::SetName(composed.names, one.number, one.shiftJisName);
	}
}

std::vector<int> Seat(BgTable::Text& composed, const std::vector<BgListOverride::Slotted>& ours)
{
	std::vector<int> seated;

	for (const BgListOverride::Slotted& one : ours)
	{
		std::string taken;

		if (BgTable::Entry(composed.list, one.number, taken))
		{
			LOG("BgListOverride: Bg_%03d is defined by a mod pack or Mods\\bg\\BgList.txt, so the stage the "
				"mod would put there waits", one.number);
			continue;
		}

		BgTable::Put(composed.list, one.number, one.entry);
		BgTable::SetName(composed.names, one.number, one.shiftJisName);
		seated.push_back(one.number);
	}

	return seated;
}

std::string Stamp(const std::vector<int>& listed)
{
	std::string joined;

	for (int number : listed)
		joined += std::to_string(number) + ",";

	char stamp[16] = {};
	sprintf_s(stamp, "%08X", Deflate::Crc32(reinterpret_cast<const uint8_t*>(joined.data()), joined.size()));

	return stamp;
}

void FollowList(const std::vector<TableLayer>& layers, const BgTable::Text& composed, const std::vector<int>& seated)
{
	std::vector<int> listed;
	const int top = BgTable::TopListing(Texts(layers), listed);
	const std::string stamp = top < 0 ? std::string() : Stamp(listed);

	if (stamp == Stored(kFollowedListKey))
		return;

	Store(kFollowedListKey, stamp);

	if (top < 0)
	{
		SaveChoices(PickerOrder::Choices());
		Store(kLearnKey, std::string());

		LOG("BgListOverride: no mod pack or Mods\\bg\\BgList.txt has a BgSelectList any more, the picker goes "
			"back to the stage table's order");
		return;
	}

	const TableLayer& followed = layers[top];
	const std::vector<int> numbers = PickerOrder::Followed(listed, followed.yours ? std::vector<int>() : seated);

	Store(kOrderKey, PickerOrder::Join(BgTable::Keys(followed.text.list, composed.list, numbers)));
	Store(kLearnKey, kOn);

	LOG("BgListOverride: %s%s has a new BgSelectList, the picker follows it", followed.folder.c_str(), kList);
}

PickerOrder::Choices ChoicesFor(const std::vector<PickerOrder::Keyed>& standard,
	const std::vector<PickerOrder::Keyed>& defined)
{
	PickerOrder::Choices choices = LoadChoices();

	if (Stored(kLearnKey) != kOn)
		return choices;

	choices = PickerOrder::Learn(standard, defined, choices.order);
	SaveChoices(choices);
	Store(kLearnKey, std::string());

	LOG("BgListOverride: the old list's order is kept, %d stage(s) hidden and %d added by it",
		static_cast<int>(choices.hidden.size()), static_cast<int>(choices.added.size()));

	return choices;
}

void NameListed(BgTable::Text& composed, const std::vector<int>& listed)
{
	for (int number : listed)
	{
		if (BgTable::HasName(composed.names, number))
			continue;

		std::string name = Stored(NameKey(number).c_str());

		if (name.empty())
			name = BgTable::Name(composed.list, number);

		if (!name.empty())
			BgTable::SetName(composed.names, number, name);
	}
}

bool WriteComposed(const char* leaf, const std::string& text, bool& changed)
{
	const std::string path = ComposedPath(leaf);
	std::string was;

	if (ReadText(path, was) && was == text)
		return true;

	CreateDirectoryTree(GetModRootPath(kComposedFolder));

	if (!WriteText(path, text))
		return false;

	changed = true;
	return true;
}

bool Commit(const BgTable::Text& composed)
{
	bool changed = false;

	if (!WriteComposed(kList, composed.list, changed) || !WriteComposed(kNames, composed.names, changed))
		return false;

	g_restart = g_restart || (changed && g_committed);
	g_committed = true;

	return true;
}

void Remember(const BgTable::Text& composed, const std::vector<int>& standard, const Inputs& inputs)
{
	AcquireSRWLockExclusive(&g_lock);

	g_composed = composed;
	g_standard = standard;
	g_inputs = inputs;
	g_synced = true;

	ReleaseSRWLockExclusive(&g_lock);
}

BgTable::Text Loaded()
{
	BgTable::Text text;

	if (ReadText(ComposedPath(kList), text.list))
	{
		ReadText(ComposedPath(kNames), text.names);
		return text;
	}

	if (!ReadText(UserPath(kList), text.list))
		Original(kList, text.list);

	return text;
}

template <typename Use>
auto WithCurrent(Use use) -> decltype(use(BgTable::Text()))
{
	AcquireSRWLockShared(&g_lock);

	if (g_synced)
	{
		auto answer = use(g_composed);
		ReleaseSRWLockShared(&g_lock);
		return answer;
	}

	ReleaseSRWLockShared(&g_lock);

	return use(Loaded());
}

}

void BgListOverride::Settle(const std::vector<int>& owned, const std::vector<std::string>& ourFiles,
	const std::vector<Reworked>& reworked)
{
	if (Layered())
		return;

	BgTable::Text old;
	ReadText(UserPath(kList), old.list);
	ReadText(UserPath(kNames), old.names);

	BgTable::Text original;

	if (!Original(kList, original.list) || !Original(kNames, original.names))
		return;

	BgTable::Text made = original;
	Rework(made, reworked);

	const std::vector<int> gameNumbers = BgTable::Numbers(original.list);
	const std::vector<int> foreign = Foreign(old.list, gameNumbers, owned, ourFiles);
	const BgTable::Text theirs = BgTable::Fragment(old, foreign);
	const BgTable::Text edits = BgTable::Edits(old, original, made, gameNumbers);
	const BgTable::Text kept = { theirs.list + edits.list, theirs.names + edits.names };

	if (!Retire(UserPath(kList)) || !Retire(UserPath(kNames)))
	{
		LOG("BgListOverride: the old Mods\\bg\\BgList.txt could not be moved aside (error %lu), so the stage "
			"table stays as it was until the next start", GetLastError());
		return;
	}

	std::vector<int> listed;

	if (BgTable::Listed(old.list, listed))
	{
		std::vector<std::string> keys;

		for (int number : listed)
			keys.push_back(BgTable::Key(old.list, number));

		Store(kOrderKey, PickerOrder::Join(keys));
		Store(kLearnKey, kOn);
	}

	if (!kept.list.empty())
		WriteText(UserPath(kList), kept.list);

	if (!kept.names.empty())
		WriteText(UserPath(kNames), kept.names);

	Store(kLayeredKey, kOn);

	LOG("BgListOverride: the stage table is composed from layers now; the old Mods\\bg\\BgList.txt %s, "
		"%d added stage(s), %d hand edit(s) of the game's stages and %d name(s) kept in a new one",
		old.list.empty() ? "was not there" : "is BgList.txt.old", static_cast<int>(foreign.size()),
		static_cast<int>(BgTable::Numbers(edits.list).size()), static_cast<int>(BgTable::Named(kept.names).size()));
}

std::vector<int> BgListOverride::Taken(const std::vector<std::string>& ourFiles)
{
	std::string original;
	Original(kList, original);

	const std::vector<int> gameNumbers = BgTable::Numbers(original);
	std::vector<int> taken;

	for (const TableLayer& layer : Layers(ourFiles))
	{
		for (int number : BgTable::Numbers(layer.text.list))
		{
			if (!Contains(gameNumbers, number) && !Contains(taken, number))
				taken.push_back(number);
		}
	}

	return taken;
}

bool BgListOverride::Sync(const std::vector<Slotted>& ours, const std::vector<int>& owned,
	const std::vector<std::string>& ourFiles, const std::vector<Reworked>& reworked)
{
	BgTable::Text composed;

	if (!Original(kList, composed.list) || !Original(kNames, composed.names))
		return false;

	const std::vector<TableLayer> layers = Layers(ourFiles);

	for (const TableLayer& layer : layers)
		BgTable::Stack(composed, layer.text);

	const std::vector<int> layered = BgTable::Numbers(composed.list);

	for (int number : owned)
	{
		if (!Contains(layered, number))
			BgTable::ClearName(composed.names, number);
	}

	Rework(composed, reworked);

	std::vector<int> standard;

	if (!BgTable::Listed(composed.list, standard))
		return false;

	const std::vector<int> seated = Seat(composed, ours);

	for (int number : seated)
	{
		if (!Contains(standard, number))
			standard.push_back(number);
	}

	FollowList(layers, composed, seated);

	const std::vector<PickerOrder::Keyed> keyedStandard = Keyed(composed.list, standard);
	const std::vector<PickerOrder::Keyed> keyedDefined = Keyed(composed.list, BgTable::Numbers(composed.list));
	const std::vector<int> arranged = PickerOrder::Arrange(keyedStandard, keyedDefined,
		ChoicesFor(keyedStandard, keyedDefined));

	NameListed(composed, arranged);
	BgTable::SetListed(composed.list, arranged);

	Remember(composed, standard, { ours, owned, ourFiles, reworked });

	LOG("BgListOverride: %d layer(s) over the game's table, %d of the mod's stages, %d listed",
		static_cast<int>(layers.size()), static_cast<int>(ours.size()), static_cast<int>(arranged.size()));

	return Commit(composed);
}

void BgListOverride::Layer(FileIndex& into)
{
	const std::string list = ComposedPath(kList);
	const std::string names = ComposedPath(kNames);

	into.Remove(FileIndex::Key(std::string(kServedList)));
	into.Remove(FileIndex::Key(std::string(kServedNames)));

	g_served = Exists(list) && Exists(names);

	if (!g_served)
		return;

	into.Add(FileIndex::Key(std::string(kServedList)), list);
	into.Add(FileIndex::Key(std::string(kServedNames)), names);
}

bool BgListOverride::Served()
{
	return g_served;
}

bool BgListOverride::IsTable(const std::string& key)
{
	return key == FileIndex::Key(std::string(kServedList)) || key == FileIndex::Key(std::string(kServedNames));
}

bool BgListOverride::Show(int number, bool shown, const std::string& shiftJisName)
{
	AcquireSRWLockShared(&g_lock);

	const bool synced = g_synced;
	const std::string key = synced ? BgTable::Key(g_composed.list, number) : std::string();
	const bool standard = Contains(g_standard, number);
	const Inputs inputs = g_inputs;

	ReleaseSRWLockShared(&g_lock);

	if (!synced)
		return false;

	PickerOrder::Choices choices = LoadChoices();
	PickerOrder::Show(choices, key, shown, standard);
	SaveChoices(choices);

	if (shown && !shiftJisName.empty())
		Store(NameKey(number).c_str(), shiftJisName);

	LOG("BgListOverride: stage %d %s the picker list", number, shown ? "joins" : "leaves");

	return Sync(inputs.ours, inputs.owned, inputs.ourFiles, inputs.reworked);
}

bool BgListOverride::IsListed(int number)
{
	return WithCurrent([number](const BgTable::Text& text)
	{
		std::vector<int> listed;

		return BgTable::Listed(text.list, listed) && Contains(listed, number);
	});
}

bool BgListOverride::Body(int number, std::string& out)
{
	return WithCurrent([number, &out](const BgTable::Text& text)
	{
		return BgTable::Entry(text.list, number, out);
	});
}

bool BgListOverride::SelectOrder(std::vector<int>& out)
{
	return WithCurrent([&out](const BgTable::Text& text)
	{
		return BgTable::Listed(text.list, out);
	});
}

bool BgListOverride::GameStageName(int number, std::string& out)
{
	out = WithCurrent([number](const BgTable::Text& text)
	{
		return BgTable::Name(text.list, number);
	});

	return !out.empty();
}

bool BgListOverride::GameNumbers(std::vector<int>& out)
{
	std::string original;

	if (!Original(kList, original))
	{
		out.clear();
		return false;
	}

	out = BgTable::Numbers(original);
	return !out.empty();
}

int BgListOverride::HighestGameCard()
{
	std::string original;

	if (!Original(kList, original))
		return -1;

	const char* const key = "StageSelTex";
	int highest = -1;

	for (size_t at = original.find(key); at != std::string::npos; at = original.find(key, at + 1))
	{
		const size_t line = original.rfind('\n', at);
		const size_t start = line == std::string::npos ? 0 : line + 1;

		if (original.find("//", start) < at)
			continue;

		const size_t equals = original.find('=', at);

		if (equals == std::string::npos)
			break;

		const int card = atoi(original.c_str() + equals + 1);
		highest = card > highest ? card : highest;
	}

	return highest;
}

bool BgListOverride::NeedsRestart()
{
	return g_restart;
}
