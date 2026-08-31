#include "Overlay/Window/MusicPanel.h"

#include "Game/BgmCatalog.h"
#include "Game/BgmControl.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmNames.h"
#include "Game/BgmTable.h"
#include "Game/BgmThemes.h"
#include "Game/CharaTables.h"
#include "Game/ModFiles.h"
#include "Game/MusicRefresh.h"
#include "Game/OstImport.h"
#include "Game/SoundpackBuilder.h"
#include "Game/SoundpackTransfer.h"
#include "Game/UserMusic.h"
#include "Overlay/ComboNav.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <imgui.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr float kComboWidth = 240.0f;
constexpr int kVanillaSource = 1;

const char* CharacterName(int chara)
{
	if (chara < 0)
		return "-";

	const char* name = CharaTables::Name(chara);
	return name != nullptr ? name : "?";
}

void DescribeTrack(int id, char* out, int size)
{
	char name[192] = {};

	if (!BgmNames::Describe(id, name, sizeof(name)))
	{
		sprintf_s(out, size, "%03d  (empty)", id);
		return;
	}

	if (BgmLibrary::IsLibraryId(id))
	{
		strncpy_s(out, size, name, _TRUNCATE);
		return;
	}

	sprintf_s(out, size, "%03d  %s", id, name);
}

void DescribeScene(int id, char* out, int size)
{
	char name[192] = {};
	const char* role = BgmTable::DescribeSlot(id);

	if (!BgmNames::Describe(id, name, sizeof(name)))
	{
		sprintf_s(out, size, "%03d  %s", id, role[0] != 0 ? role : "(empty)");
		return;
	}

	if (role[0] == 0)
	{
		sprintf_s(out, size, "%03d  %s", id, name);
		return;
	}

	sprintf_s(out, size, "%s  -  %s", role, name);
}

void DescribeRule(const BgmRules::Rule& rule, char* out, int size)
{
	char track[224] = {};
	DescribeTrack(rule.bgm, track, sizeof(track));

	switch (rule.kind)
	{
	case BgmRules::Kind_Matchup:
		sprintf_s(out, size, "%s %s %s  ->  %s", CharacterName(rule.a),
			rule.bothWays ? "vs" : "over", CharacterName(rule.b), track);
		return;
	case BgmRules::Kind_Character:
		sprintf_s(out, size, "%s is in the match  ->  %s", CharacterName(rule.a), track);
		return;
	default:
		break;
	}

	char source[224] = {};
	DescribeScene(rule.a, source, sizeof(source));
	sprintf_s(out, size, "%s  ->  %s", source, track);
}

void Lower(const char* text, char* out, int size)
{
	int i = 0;

	for (; text[i] != 0 && i + 1 < size; ++i)
		out[i] = static_cast<char>(tolower(static_cast<unsigned char>(text[i])));

	out[i] = 0;
}

bool Matches(const char* haystack, const char* needle)
{
	if (needle[0] == 0)
		return true;

	char a[224] = {};
	char b[64] = {};
	Lower(haystack, a, sizeof(a));
	Lower(needle, b, sizeof(b));

	return strstr(a, b) != nullptr;
}

const char* SourceTag(int id)
{
	const BgmLibrary::Track* track = BgmLibrary::Get(id);
	return track != nullptr ? track->tag : "UNI2";
}

bool IsScene(int id)
{
	return id >= 0 && id < 100 && BgmTable::IsPresent(id);
}

void DescribeEntry(int id, bool sceneOnly, char* out, int size)
{
	if (sceneOnly)
	{
		DescribeScene(id, out, size);
		return;
	}

	DescribeTrack(id, out, size);
}

bool IsSelectable(int id, bool sceneOnly)
{
	return sceneOnly ? IsScene(id) : BgmCatalog::IsListed(id);
}

int StepListed(int from, int steps, bool sceneOnly)
{
	std::vector<int> pool;

	const int total = sceneOnly ? BgmTable::kSlotCount : BgmCatalog::Count();

	for (int index = 0; index < total; ++index)
	{
		const int id = BgmCatalog::IdAt(index);

		if (sceneOnly ? IsScene(id) : BgmCatalog::IsListed(id))
			pool.push_back(id);
	}

	for (size_t i = 0; i < pool.size(); ++i)
	{
		if (pool[i] != from)
			continue;

		const int target = static_cast<int>(i) + steps;

		if (target < 0 || target >= static_cast<int>(pool.size()))
			return from;

		return pool[target];
	}

	return from;
}

}

void MusicPanel::Draw()
{
	if (!BgmControl::IsHooked())
	{
		UiText::Warn("Music control is not active: %s", BgmControl::GetStatusText());
		return;
	}

	DrawStatus();
	ImGui::Separator();

	if (!ImGui::BeginTabBar("##music"))
		return;

	if (ImGui::BeginTabItem("Soundpacks"))
	{
		DrawSoundpacks();
		ImGui::Separator();
		DrawGetOst();
		ImGui::Separator();
		DrawTransfer();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Add music"))
	{
		DrawMyMusic();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Browse"))
	{
		DrawBrowse();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Rules"))
	{
		DrawRules();
		ImGui::Separator();
		DrawRuleEditor();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void MusicPanel::DrawStatus()
{
	if (BgmControl::IsSuppressed())
	{
		UiText::Warn("The game has BGM switched off, so nothing will play.");
		return;
	}

	const int playing = BgmControl::Current();

	char playingText[224] = {};

	if (playing >= 0)
		DescribeTrack(playing, playingText, sizeof(playingText));
	else
		sprintf_s(playingText, "silence");

	if (BgmControl::IsPinned())
		UiText::Good("Your pick: %s", playingText);
	else
		ImGui::Text("Playing: %s", playingText);

	ImGui::SameLine();

	ImGui::BeginDisabled(playing < 0);

	if (ImGui::SmallButton("Stop"))
		BgmControl::Stop();

	ImGui::EndDisabled();

	if (BgmControl::IsPinned())
	{
		ImGui::SameLine();

		if (ImGui::SmallButton("Give it back"))
			BgmControl::Release();

		UiText::Muted("The game's own music is on hold until you stop or give it back.");
	}

	ImGui::Text("Loaded characters: %s vs %s", CharacterName(BgmControl::GetCharacter(0)),
		CharacterName(BgmControl::GetCharacter(1)));
}

void MusicPanel::DrawSoundpacks()
{
	const int count = BgmThemes::Count();
	const int active = BgmThemes::ActiveIndex();

	if (count == 0)
	{
		UiText::Muted("No soundpacks installed.");
		UiText::Help("A soundpack is a folder with a theme.ini in it. The ini names the pack and "
			"lists which scene each of its tracks stands in for, one 'scene = track' line per "
			"track under a [Map] section. A track is either a vanilla slot number or the file name "
			"of a track from a pack in the library folder.");
		ImGui::TextWrapped("Looked in: %s", BgmThemes::ThemesPath());

		if (ImGui::Button("Rescan folder"))
			MusicRefresh::Rescan();

		return;
	}

	UiText::Help("Applying a soundpack points every screen at that game's music. It rewrites the "
		"replace rules and nothing else, so switching is instant and rules you added yourself are "
		"left alone.");

	const int apply = DrawSoundpackTable(count, active);

	DrawSoundpackControls(active);

	if (apply >= 0)
		BgmThemes::Apply(apply);
}

bool MusicPanel::DrawSoundpackRow(const BgmThemes::Theme& theme, bool isActive)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	if (isActive)
		UiText::Good("%s", theme.name);
	else
		ImGui::TextUnformatted(theme.name);

	if (theme.notes[0] != 0)
		UiText::Muted("%s", theme.notes);

	ImGui::TableNextColumn();

	if (theme.readyCount == theme.entryCount)
		ImGui::Text("%d", theme.entryCount);
	else
		UiText::Warn("%d of %d", theme.readyCount, theme.entryCount);

	ImGui::TableNextColumn();

	ImGui::BeginDisabled(theme.readyCount == 0);

	const bool apply = ImGui::SmallButton(isActive ? "Reapply" : "Apply");

	ImGui::EndDisabled();
	return apply;
}

int MusicPanel::DrawSoundpackTable(int count, int active)
{
	if (!ImGui::BeginTable("##bgmthemes", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(140.0f))))
	{
		return -1;
	}

	ImGui::TableSetupColumn("Soundpack");
	ImGui::TableSetupColumn("Scenes", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(90.0f));
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(90.0f));
	ImGui::TableHeadersRow();

	int apply = -1;

	for (int i = 0; i < count; ++i)
	{
		const BgmThemes::Theme* theme = BgmThemes::Get(i);

		if (theme == nullptr)
			continue;

		ImGui::PushID(i);

		if (DrawSoundpackRow(*theme, i == active))
			apply = i;

		ImGui::PopID();
	}

	ImGui::EndTable();
	return apply;
}

void MusicPanel::DrawSoundpackControls(int active)
{
	if (ImGui::Button("Rescan folder"))
		MusicRefresh::Rescan();

	ImGui::SameLine();

	ImGui::BeginDisabled(active < 0);

	if (ImGui::Button("Back to vanilla"))
		BgmThemes::Clear();

	ImGui::EndDisabled();

	if (ModFiles::Count() == 0)
	{
		UiText::Warn("Mods folder: %s", ModFiles::StatusText());
		return;
	}

	UiText::Muted("Mods folder: %s, %d read so far", ModFiles::StatusText(), ModFiles::Hits());
}

void MusicPanel::DrawPackBuilder()
{
	if (!SoundpackBuilder::IsOpen())
	{
		if (ImGui::Button("New soundpack"))
			SoundpackBuilder::Begin();

		ImGui::SameLine();
		UiText::Help("Start one and a Pack column appears in the list below. Tick the tracks you "
			"want in it, choose which screen each one plays on, and save.");

		if (m_packStatus[0] != 0)
			UiText::Muted("%s", m_packStatus);

		return;
	}

	DrawPackDraft();
}

void MusicPanel::DrawPackDraft()
{
	Ui::SetItemWidth(kComboWidth);
	ImGui::InputTextWithHint("Name", "Soundpack name", SoundpackBuilder::NameBuffer(),
		SoundpackBuilder::NameCapacity());

	ImGui::SameLine();

	Ui::SetItemWidth(kComboWidth);
	ImGui::InputTextWithHint("Author", "You", SoundpackBuilder::AuthorBuffer(),
		SoundpackBuilder::AuthorCapacity());

	const int count = SoundpackBuilder::Count();

	if (count == 0)
		UiText::Warn("Nothing picked yet. Tick Pack on the tracks you want in it.");
	else
		DrawPackDraftTable(count);

	ImGui::BeginDisabled(count == 0);

	if (ImGui::Button("Save soundpack"))
		SoundpackBuilder::Save(m_packStatus, sizeof(m_packStatus));

	ImGui::EndDisabled();

	ImGui::SameLine();

	if (ImGui::Button("Cancel"))
	{
		SoundpackBuilder::Cancel();
		m_packStatus[0] = 0;
	}

	if (m_packStatus[0] != 0)
		UiText::Warn("%s", m_packStatus);
}

void MusicPanel::DrawPackDraftTable(int count)
{
	if (!ImGui::BeginTable("##packdraft", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(130.0f))))
	{
		return;
	}

	ImGui::TableSetupColumn("Track");
	ImGui::TableSetupColumn("Plays on");
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(60.0f));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	int removeIndex = -1;

	for (int i = 0; i < count; ++i)
	{
		ImGui::PushID(i);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		char track[224] = {};
		DescribeTrack(SoundpackBuilder::TrackAt(i), track, sizeof(track));
		ImGui::TextUnformatted(track);

		ImGui::TableNextColumn();
		int scene = SoundpackBuilder::SceneAt(i);

		if (DrawSceneCombo(scene))
			SoundpackBuilder::SetScene(i, scene);

		ImGui::TableNextColumn();

		if (ImGui::SmallButton("Drop"))
			removeIndex = i;

		ImGui::PopID();
	}

	ImGui::EndTable();

	if (removeIndex >= 0)
		SoundpackBuilder::RemoveAt(removeIndex);
}

void MusicPanel::DrawMyMusic()
{
	UiText::Muted("Add your own music");
	UiText::Help("Pick an MP3, OGG or WAV and it is copied into the mod and listed in Browse. The "
		"game will open a loose file only when it is OGG Vorbis, so anything else is re-encoded on "
		"the way in - the converted copy sits in Music\\.cache and can be deleted at any time.");

	Ui::SetItemWidth(kComboWidth);
	ImGui::InputTextWithHint("Goes in", "Folder name", m_musicPack, IM_ARRAYSIZE(m_musicPack));

	if (ImGui::Button("Import music"))
		m_musicDialog.BeginOpen("Pick the music to add", UserMusic::SupportedFilter());

	ImGui::SameLine();

	bool rescan = ImGui::Button("Rescan music folder");

	std::string picked;

	if (m_musicDialog.TakeResult(picked))
		rescan = UserMusic::Import(picked, m_musicPack, m_musicStatus, sizeof(m_musicStatus));

	if (rescan)
		MusicRefresh::Rescan();

	if (m_musicStatus[0] != 0)
		UiText::Muted("%s", m_musicStatus);

	RefreshMusicList();

	if (m_music.empty())
	{
		UiText::Muted("Nothing in %s yet.", UserMusic::Root().c_str());
		return;
	}

	DrawMyMusicTable();
}

void MusicPanel::RefreshMusicList()
{
	const int version = UserMusic::Version();

	if (version != m_musicVersion)
	{
		m_music = UserMusic::Snapshot();
		m_musicVersion = version;
	}

	int pending = 0;

	for (const UserMusic::Entry& entry : m_music)
	{
		if (entry.status == UserMusic::Status_Converting)
			++pending;
	}

	if (pending > 0)
		UiText::Warn("Converting %d file(s)...", pending);
}

void MusicPanel::DrawMyMusicRow(const UserMusic::Entry& entry)
{
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	UiText::Muted("%s", entry.pack.c_str());

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(entry.fileName.c_str());

	ImGui::TableNextColumn();

	if (entry.status == UserMusic::Status_Ready)
		UiText::Good("plays");
	else if (entry.status == UserMusic::Status_Converting)
		UiText::Warn("working");
	else
		UiText::Warn("skipped");

	ImGui::TableNextColumn();

	float loop = static_cast<float>(entry.loopPos);

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputFloat("##loop", &loop, 0.0f, 0.0f, "%.3f");

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		UserMusic::SetLoopPoint(entry.slotName, loop);
		BgmLibrary::Load();
	}

	ImGui::TableNextColumn();
	UiText::Muted("%s", entry.note.c_str());
}

void MusicPanel::DrawMyMusicTable()
{
	UiText::Help("Loop from is where the track restarts when it reaches the end, in seconds. Leave "
		"it at 0 and the whole thing repeats, intro and all; set it past the intro and the loop "
		"sounds like the game's own music. This is what a soundpack track carries as its loop "
		"point.");

	if (!ImGui::BeginTable("##usermusic", 5,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(190.0f))))
	{
		return;
	}

	ImGui::TableSetupColumn("Folder", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(90.0f));
	ImGui::TableSetupColumn("File");
	ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(70.0f));
	ImGui::TableSetupColumn("Loop from", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(80.0f));
	ImGui::TableSetupColumn("Notes");
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	int row = 0;

	for (const UserMusic::Entry& entry : m_music)
	{
		ImGui::PushID(row++);
		DrawMyMusicRow(entry);
		ImGui::PopID();
	}

	ImGui::EndTable();
}

void MusicPanel::DrawGetOst()
{
	UiText::Muted("Take the soundtrack from another game you own");
	UiText::Help("Pick the folder that holds UNIclr.exe or UNIst.exe, MBTL.exe, or MBAA.exe. The "
		"mod reads that game's music out of your own copy and installs it here as a soundpack, "
		"with song titles and loop points. Nothing is downloaded, and running it twice on the "
		"same game replaces what it added rather than doubling it.");

	ImGui::BeginDisabled(OstImport::IsBusy());

	if (ImGui::Button("Get OST from French-Bread games"))
		m_ostDialog.BeginFolder("Pick the game folder to take the soundtrack from");

	ImGui::EndDisabled();

	std::string picked;

	if (m_ostDialog.TakeResult(picked))
	{
		const OstImport::Source source = OstImport::Detect(picked.c_str());

		if (OstImport::IsSupported(source))
			OstImport::Begin(picked.c_str());
		else
			UiText::Warn("That folder holds %s.", OstImport::SourceName(source));
	}

	if (OstImport::IsBusy())
	{
		ImGui::ProgressBar(OstImport::Progress() / 100.0f, ImVec2(Ui::Scaled(240.0f), 0.0f));
		UiText::Warn("%s", OstImport::StatusText());
		return;
	}

	if (OstImport::Progress() > 0)
		UiText::Muted("%s", OstImport::StatusText());
}

void MusicPanel::DrawTransfer()
{
	UiText::Muted("Send your soundpacks to someone else");
	UiText::Help("Export writes one zip holding everything a friend needs: the audio, the slot "
		"table, the picker list and every soundpack. Import puts one back. Anything you dropped "
		"in the Music folder yourself goes along too.");

	ImGui::BeginDisabled(SoundpackTransfer::IsBusy());

	if (ImGui::Button("Export soundpacks"))
	{
		m_exportDialog.BeginSave("Export soundpacks", "Zip archive\0*.zip\0\0",
			"UNI2-IM-Soundpacks.zip");
	}

	ImGui::SameLine();

	if (ImGui::Button("Import soundpacks"))
		m_importDialog.BeginOpen("Import soundpacks", "Zip archive\0*.zip\0\0");

	ImGui::EndDisabled();

	std::string picked;

	if (m_exportDialog.TakeResult(picked))
		SoundpackTransfer::Export(picked.c_str());

	if (m_importDialog.TakeResult(picked))
		SoundpackTransfer::Import(picked.c_str());

	if (SoundpackTransfer::IsBusy())
		UiText::Warn("%s", SoundpackTransfer::StatusText());
	else
		UiText::Muted("%s", SoundpackTransfer::StatusText());
}

void MusicPanel::DrawSourceFilter()
{
	char current[32] = "Everything";

	if (m_source == kVanillaSource)
		strncpy_s(current, "UNI2", _TRUNCATE);
	else if (m_source > kVanillaSource)
		strncpy_s(current, BgmLibrary::TagAt(m_source - kVanillaSource - 1), _TRUNCATE);

	Ui::SetItemWidth(kComboWidth * 0.6f);

	if (ImGui::BeginCombo("##source", current))
	{
		if (ImGui::Selectable("Everything", m_source == 0))
			m_source = 0;

		if (ImGui::Selectable("UNI2", m_source == kVanillaSource))
			m_source = kVanillaSource;

		for (int i = 0; i < BgmLibrary::TagCount(); ++i)
		{
			const int index = kVanillaSource + 1 + i;

			if (ImGui::Selectable(BgmLibrary::TagAt(i), m_source == index))
				m_source = index;
		}

		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();
	const int target = m_source + steps;

	if (steps != 0 && target >= 0 && target <= kVanillaSource + BgmLibrary::TagCount())
		m_source = target;
}

bool MusicPanel::PassesFilter(int id, const char* name) const
{
	if (m_source == kVanillaSource && BgmLibrary::IsLibraryId(id))
		return false;

	if (m_source > kVanillaSource)
	{
		const BgmLibrary::Track* track = BgmLibrary::Get(id);

		if (track == nullptr)
			return false;

		if (strcmp(track->tag, BgmLibrary::TagAt(m_source - kVanillaSource - 1)) != 0)
			return false;
	}

	return Matches(name, m_search);
}

void MusicPanel::DrawShuffle()
{
	bool shuffle = BgmCatalog::ShuffleEnabled();

	if (ImGui::Checkbox("Randomizer", &shuffle))
		BgmCatalog::SetShuffleEnabled(shuffle);

	UiText::Help("While this is on, every time the game asks for music it gets a random track "
		"from this list instead. Rules are ignored until you turn it off. Tracks you switch off "
		"below are never picked.");

	ImGui::SameLine();

	if (ImGui::SmallButton("Enable all"))
		BgmCatalog::SetAllAllowed(true);

	ImGui::SameLine();

	if (ImGui::SmallButton("Disable all"))
		BgmCatalog::SetAllAllowed(false);

	ImGui::SameLine();
	UiText::Muted("%d in the draw", BgmCatalog::AllowedCount());
}

void MusicPanel::DrawBrowse()
{
	DrawPackBuilder();
	ImGui::Separator();
	DrawShuffle();

	Ui::SetItemWidth(kComboWidth);
	ImGui::InputTextWithHint("##search", "Search by name", m_search, IM_ARRAYSIZE(m_search));

	ImGui::SameLine();
	DrawSourceFilter();

	ImGui::SameLine();
	DrawTrackCount();

	UiText::Muted("Play starts a track and holds it. The game gets its music back when you press "
		"Stop.");

	DrawTrackTable();
}

void MusicPanel::DrawTrackCount()
{
	int shown = 0;
	int total = 0;

	for (int index = 0; index < BgmCatalog::Count(); ++index)
	{
		const int id = BgmCatalog::IdAt(index);

		if (!BgmCatalog::IsListed(id))
			continue;

		++total;

		char name[224] = {};

		if (BgmNames::Describe(id, name, sizeof(name)) && PassesFilter(id, name))
			++shown;
	}

	if (shown == total)
	{
		UiText::Muted("%d tracks", total);
		return;
	}

	UiText::Muted("%d of %d tracks", shown, total);
}

void MusicPanel::SetUpTrackColumns(bool building)
{
	if (building)
		ImGui::TableSetupColumn("Pack", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(38.0f));

	ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(30.0f));
	ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(56.0f));
	ImGui::TableSetupColumn("Track");
	ImGui::TableSetupColumn("Where it plays", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(150.0f));
	ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(70.0f));
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(56.0f));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();
}

void MusicPanel::DrawTrackRow(int id, const char* name, bool building, bool playing)
{
	ImGui::TableNextRow();

	if (building)
	{
		ImGui::TableNextColumn();

		bool inPack = SoundpackBuilder::Holds(id);

		if (ImGui::Checkbox("##pack", &inPack))
			SoundpackBuilder::Toggle(id);
	}

	ImGui::TableNextColumn();

	bool allowed = BgmCatalog::IsAllowed(id);

	if (ImGui::Checkbox("##on", &allowed))
		BgmCatalog::SetAllowed(id, allowed);

	ImGui::TableNextColumn();
	UiText::Muted("%s", SourceTag(id));

	ImGui::TableNextColumn();

	if (playing)
		UiText::Good("%s", name);
	else if (allowed)
		ImGui::TextUnformatted(name);
	else
		UiText::Muted("%s", name);

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(BgmLibrary::IsLibraryId(id) ? "-" : BgmTable::DescribeSlot(id));

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(BgmLibrary::Loops(id) ? "yes" : "no");

	ImGui::TableNextColumn();

	if (ImGui::SmallButton("Play"))
		BgmControl::Play(id);
}

void MusicPanel::DrawTrackTable()
{
	const bool building = SoundpackBuilder::IsOpen();

	if (!ImGui::BeginTable(building ? "##bgmtrackspack" : "##bgmtracks", building ? 7 : 6,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
		ImVec2(0.0f, Ui::Scaled(280.0f))))
	{
		return;
	}

	SetUpTrackColumns(building);

	const int playing = BgmControl::Current();

	for (int index = 0; index < BgmCatalog::Count(); ++index)
	{
		const int id = BgmCatalog::IdAt(index);

		if (!BgmCatalog::IsListed(id))
			continue;

		char name[224] = {};

		if (!BgmNames::Describe(id, name, sizeof(name)))
			continue;

		if (!PassesFilter(id, name))
			continue;

		ImGui::PushID(index);
		DrawTrackRow(id, name, building, id == playing);
		ImGui::PopID();
	}

	ImGui::EndTable();
}

void MusicPanel::DrawRules()
{
	bool enabled = BgmRules::IsEnabled();

	if (ImGui::Checkbox("Apply my music rules", &enabled))
	{
		BgmRules::SetEnabled(enabled);
		BgmRules::Save();
	}

	UiText::Help("A rule swaps one track for another. Matchup and character rules only fire in "
		"battle; a Replace rule can take over any screen. Off means the game plays what it "
		"normally would. The randomizer in Browse overrides all of this while it is on.");

	DrawRuleTransfer();

	const int count = BgmRules::Count();

	if (count == 0)
	{
		UiText::Muted("No rules yet. The game ships exactly three matchup themes; this is the same "
			"idea without the limit.");
		return;
	}

	const int removeIndex = DrawRuleTable(count);

	if (removeIndex < 0)
		return;

	BgmRules::Remove(removeIndex);
	BgmRules::Save();
}

void MusicPanel::DrawRuleTransfer()
{
	ImGui::BeginDisabled(BgmRules::Count() == 0);

	if (ImGui::SmallButton("Export rules"))
		m_rulesExportDialog.BeginSave("Export rules", "Rule list\0*.txt\0\0", "bgm-rules.txt");

	ImGui::EndDisabled();

	ImGui::SameLine();

	if (ImGui::SmallButton("Import rules"))
		m_rulesImportDialog.BeginOpen("Import rules", "Rule list\0*.txt\0\0");

	ImGui::SameLine();
	UiText::Help("Export writes your rule list to a text file you can send. Import adds the rules "
		"in one to what you already have - it does not wipe them.");

	std::string rulesPath;

	if (m_rulesExportDialog.TakeResult(rulesPath))
		BgmRules::ExportTo(rulesPath.c_str());

	if (m_rulesImportDialog.TakeResult(rulesPath))
		BgmRules::ImportFrom(rulesPath.c_str());
}

bool MusicPanel::DrawRuleRow(int index, const BgmRules::Rule& stored)
{
	BgmRules::Rule rule = stored;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	if (ImGui::Checkbox("##on", &rule.enabled))
	{
		BgmRules::Update(index, rule);
		BgmRules::Save();
	}

	ImGui::TableNextColumn();

	if (rule.fromTheme)
		UiText::Muted("Theme");
	else
		ImGui::TextUnformatted(BgmRules::KindName(rule.kind));

	ImGui::TableNextColumn();
	char text[512] = {};
	DescribeRule(rule, text, sizeof(text));
	ImGui::TextUnformatted(text);

	ImGui::TableNextColumn();

	if (ImGui::SmallButton("Preview"))
		BgmControl::Play(rule.bgm);

	ImGui::SameLine();
	return ImGui::SmallButton("Remove");
}

int MusicPanel::DrawRuleTable(int count)
{
	if (!ImGui::BeginTable("##bgmrules", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(180.0f))))
	{
		return -1;
	}

	ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(34.0f));
	ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(90.0f));
	ImGui::TableSetupColumn("Rule");
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(110.0f));
	ImGui::TableHeadersRow();

	int removeIndex = -1;

	for (int i = 0; i < count; ++i)
	{
		const BgmRules::Rule* stored = BgmRules::Get(i);

		if (stored == nullptr)
			continue;

		ImGui::PushID(i);

		if (DrawRuleRow(i, *stored))
			removeIndex = i;

		ImGui::PopID();
	}

	ImGui::EndTable();
	return removeIndex;
}

void MusicPanel::DrawRuleEditor()
{
	ImGui::TextUnformatted("Add a rule");

	const char* const kinds[] = { "Matchup", "Character", "Any scene or track" };

	Ui::SetItemWidth(kComboWidth);
	ImGui::Combo("When", &m_draft.kind, kinds, IM_ARRAYSIZE(kinds));

	if (m_draft.kind == BgmRules::Kind_Matchup)
	{
		DrawCharacterCombo("Left", m_draft.a);
		DrawCharacterCombo("Right", m_draft.b);
		ImGui::Checkbox("Either side of the screen", &m_draft.bothWays);
	}
	else if (m_draft.kind == BgmRules::Kind_Character)
	{
		DrawCharacterCombo("Character", m_draft.a);
	}
	else
	{
		UiText::Help("Every screen with music is in this list: character select, the main menu, "
			"the network menu, the VS screen, win demo, continue, game over, the story tracks and "
			"every battle theme.");
		DrawTrackCombo("Replace", m_draft.a, true);
	}

	DrawTrackCombo("Play", m_draft.bgm);

	const bool valid = BgmLibrary::IsPlayable(m_draft.bgm);

	if (!valid)
		UiText::Warn("There is no track behind that pick.");

	ImGui::BeginDisabled(!valid);

	if (ImGui::Button("Add rule"))
	{
		m_draft.enabled = true;
		m_draft.fromTheme = false;

		if (BgmRules::Add(m_draft))
			BgmRules::Save();
	}

	ImGui::EndDisabled();
}

bool MusicPanel::DrawCharacterCombo(const char* label, int& chara)
{
	const int count = CharaTables::GetCharaCount();

	if (chara < 0 || chara >= count)
		chara = 0;

	Ui::SetItemWidth(kComboWidth);

	bool changed = false;

	if (ImGui::BeginCombo(label, CharacterName(chara)))
	{
		for (int i = 0; i < count; ++i)
		{
			const bool selected = i == chara;

			if (ImGui::Selectable(CharacterName(i), selected))
			{
				chara = i;
				changed = true;
			}

			ComboNav::KeepSelectedInView(selected);
		}

		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();
	const int target = chara + steps;

	if (steps != 0 && target >= 0 && target < count)
	{
		chara = target;
		changed = true;
	}

	return changed;
}

bool MusicPanel::DrawSceneCombo(int& scene)
{
	char current[224] = {};
	DescribeScene(scene, current, sizeof(current));

	ImGui::SetNextItemWidth(-1.0f);

	bool changed = false;

	if (!ImGui::BeginCombo("##scene", current))
		return false;

	for (int id = 0; id < BgmTable::kSlotCount; ++id)
	{
		if (!IsScene(id))
			continue;

		char text[224] = {};
		DescribeScene(id, text, sizeof(text));

		const bool selected = id == scene;

		ImGui::PushID(id);

		if (ImGui::Selectable(text, selected))
		{
			scene = id;
			changed = true;
		}

		ComboNav::KeepSelectedInView(selected);

		ImGui::PopID();
	}

	ImGui::EndCombo();
	return changed;
}

bool MusicPanel::DrawTrackComboList(bool sceneOnly, int& bgm)
{
	if (ImGui::IsWindowAppearing())
	{
		m_pick[0] = 0;
		ImGui::SetKeyboardFocusHere();
	}

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##pick", "Search", m_pick, IM_ARRAYSIZE(m_pick));

	ImGui::Separator();

	const int total = sceneOnly ? BgmTable::kSlotCount : BgmCatalog::Count();

	bool changed = false;

	for (int index = 0; index < total; ++index)
	{
		const int id = BgmCatalog::IdAt(index);

		if (!IsSelectable(id, sceneOnly))
			continue;

		char text[224] = {};
		DescribeEntry(id, sceneOnly, text, sizeof(text));

		if (!Matches(text, m_pick))
			continue;

		const bool selected = id == bgm;

		ImGui::PushID(index);

		if (ImGui::Selectable(text, selected))
		{
			bgm = id;
			changed = true;
		}

		ComboNav::KeepSelectedInView(selected);

		ImGui::PopID();
	}

	return changed;
}

bool MusicPanel::DrawTrackCombo(const char* label, int& bgm, bool sceneOnly)
{
	if (!IsSelectable(bgm, sceneOnly))
		bgm = 1;

	char current[224] = {};
	DescribeEntry(bgm, sceneOnly, current, sizeof(current));

	Ui::SetItemWidth(kComboWidth * 1.6f);

	bool changed = false;

	if (ImGui::BeginCombo(label, current))
	{
		changed = DrawTrackComboList(sceneOnly, bgm);
		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();

	if (steps == 0)
		return changed;

	const int next = StepListed(bgm, steps, sceneOnly);

	if (next == bgm)
		return changed;

	bgm = next;
	return true;
}
