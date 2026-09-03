#include "Overlay/Window/SoundWindow.h"

#include "Game/CharaSounds.h"
#include "Game/CharaTables.h"
#include "Game/SoundPacks.h"
#include "Game/SoundsReadme.h"
#include "Game/VoiceImport.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <imgui.h>

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kGame = "The game's own";
constexpr const char* kZipFilter = "Sound pack (*.zip)\0*.zip\0All files\0*.*\0";
constexpr const char* kAudioFilter =
	"Sound (*.ogg;*.wav;*.mp3)\0*.ogg;*.wav;*.mp3\0All files\0*.*\0";
constexpr const char* kNewPackPopup = "Make a pack";

const SoundPacks::Pack* PackById(const char* id)
{
	for (int i = 0; i < SoundPacks::Count(); ++i)
	{
		const SoundPacks::Pack* const pack = SoundPacks::Get(i);

		if (pack != nullptr && pack->id == id)
			return pack;
	}

	return nullptr;
}

const char* LabelFor(const char* id)
{
	if (id == nullptr || id[0] == '\0')
		return kGame;

	const SoundPacks::Pack* const pack = PackById(id);

	return pack != nullptr ? pack->name.c_str() : id;
}

bool Holds(const std::string& text, const char* needle)
{
	if (needle[0] == '\0')
		return true;

	const size_t length = strlen(needle);

	if (text.size() < length)
		return false;

	for (size_t at = 0; at + length <= text.size(); ++at)
	{
		if (_strnicmp(text.c_str() + at, needle, length) == 0)
			return true;
	}

	return false;
}

std::string NameOf(const std::string& path)
{
	const size_t slash = path.find_last_of("\\/");

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool WheelPicked(int& value, int count)
{
	if (count <= 0 || !ImGui::IsItemHovered())
		return false;

	const float wheel = ImGui::GetIO().MouseWheel;

	if (wheel == 0.0f)
		return false;

	ImGui::GetIO().MouseWheel = 0.0f;

	const int next = value + (wheel > 0.0f ? -1 : 1);

	if (next < 0 || next >= count || next == value)
		return false;

	value = next;
	return true;
}

}

SoundWindow::SoundWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void SoundWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(780.0f), Ui::Scaled(560.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(460.0f), Ui::Scaled(320.0f)),
		ImVec2(FLT_MAX, FLT_MAX));
}

void SoundWindow::Draw()
{
	PumpDialogs();

	if (m_status[0] != '\0')
		UiText::Muted("%s", m_status);

	if (!ImGui::BeginTabBar("sound"))
		return;

	if (ImGui::BeginTabItem("Replace"))
	{
		DrawReplace();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Packs"))
	{
		DrawPacks();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Help"))
	{
		DrawHowTo();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void SoundWindow::Reload()
{
	CharaSounds::Load(m_chara);
}

void SoundWindow::DrawPicker()
{
	const int count = CharaTables::GetCharaCount();
	int picked = m_chara;

	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));

	if (ImGui::BeginCombo("##chara", CharaTables::Name(m_chara)))
	{
		for (int chara = 0; chara < count; ++chara)
		{
			if (ImGui::Selectable(CharaTables::Name(chara), chara == m_chara))
				picked = chara;
		}

		ImGui::EndCombo();
	}

	WheelPicked(picked, count);

	if (picked != m_chara)
	{
		m_chara = picked;
		Reload();
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(Ui::Scaled(220.0f));
	DrawChooser("##pack");

	ImGui::SameLine();
	ImGui::BeginDisabled(CharaSounds::IsLoading());

	if (ImGui::Button(CharaSounds::Count() == 0 ? "Load voices and sounds" : "Reload"))
		Reload();

	ImGui::EndDisabled();

	if (CharaSounds::Count() != 0)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Ui::Scaled(150.0f));
		ImGui::InputTextWithHint("##filter", "search", m_filter, sizeof(m_filter));
	}

	if (CharaSounds::StatusText()[0] != '\0')
		UiText::Muted("%s", CharaSounds::StatusText());
}

bool SoundWindow::Wanted(int index) const
{
	const CharaSounds::Entry* const entry = CharaSounds::Get(index);

	if (entry == nullptr)
		return false;

	return Holds(entry->file, m_filter) || Holds(entry->note, m_filter) ||
		Holds(entry->group, m_filter);
}

void SoundWindow::AskForPack(int index)
{
	m_wanted = index;

	if (SoundPacks::ChoiceFor(m_chara)[0] != '\0')
	{
		m_replacing = index;
		m_replace.BeginOpen("Choose a sound to use instead", kAudioFilter);
		return;
	}

	sprintf_s(m_packName, "%s - mine", CharaTables::Name(m_chara));
	m_askPack = true;
}

void SoundWindow::DrawRow(int index)
{
	const CharaSounds::Entry* const entry = CharaSounds::Get(index);

	if (entry == nullptr)
		return;

	const CharaSounds::State state = entry->state;

	ImGui::PushID(index);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(entry->group.c_str());

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(entry->stem.c_str());

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(entry->note.c_str());

	ImGui::TableNextColumn();

	if (state == CharaSounds::State_Pack)
		UiText::Good("%s", NameOf(entry->yours).c_str());
	else
		UiText::Muted("the game's own");

	ImGui::TableNextColumn();

	if (ImGui::Button("Play"))
		CharaSounds::Play(index, m_status, sizeof(m_status));

	ImGui::SameLine();

	if (ImGui::Button("New...") && !m_replace.IsRunning())
		AskForPack(index);

	if (state == CharaSounds::State_Pack)
	{
		ImGui::SameLine();

		if (ImGui::Button("Back to original"))
			CharaSounds::UseGame(index, m_status, sizeof(m_status));
	}

	ImGui::PopID();
}

void SoundWindow::UpdateVisible()
{
	if (m_revision != SoundPacks::Revision())
	{
		m_revision = SoundPacks::Revision();
		CharaSounds::Restate();
	}

	const int count = CharaSounds::Count();
	const int chara = CharaSounds::LoadedChara();

	if (count == m_visibleFor && chara == m_visibleChara &&
		strcmp(m_filter, m_visibleFilter) == 0)
	{
		return;
	}

	m_visibleFor = count;
	m_visibleChara = chara;
	strncpy_s(m_visibleFilter, m_filter, _TRUNCATE);

	m_visible.clear();

	for (int index = 0; index < count; ++index)
	{
		if (Wanted(index))
			m_visible.push_back(index);
	}
}

void SoundWindow::DrawList()
{
	UpdateVisible();

	const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

	if (!ImGui::BeginTable("sounds", 5, flags, Ui::Scaled(0.0f, 400.0f)))
		return;

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Where", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(110.0f));
	ImGui::TableSetupColumn("Sound", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(150.0f));
	ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Playing", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(150.0f));
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(230.0f));
	ImGui::TableHeadersRow();

	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(m_visible.size()));

	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			DrawRow(m_visible[row]);
	}

	ImGui::EndTable();
}

void SoundWindow::DrawNewPack()
{
	if (m_askPack)
	{
		ImGui::OpenPopup(kNewPackPopup);
		m_askPack = false;
	}

	if (!ImGui::BeginPopupModal(kNewPackPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	UiText::Muted("%s is still using the game's own sounds. Your changes have to live in a pack, "
		"so give this one a name - it is the folder it will sit in, and what you would send to a "
		"friend.", CharaTables::Name(m_chara));

	ImGui::Spacing();
	ImGui::SetNextItemWidth(Ui::Scaled(280.0f));
	ImGui::InputText("##packname", m_packName, sizeof(m_packName));

	ImGui::Spacing();

	if (ImGui::Button("Make it", Ui::Scaled(110.0f, 0.0f)))
	{
		std::string id;

		if (SoundPacks::Create(m_packName, m_chara, id, m_status, sizeof(m_status)))
		{
			SoundPacks::Choose(m_chara, id);
			CharaSounds::Restate();

			if (m_wanted >= 0 && !m_replace.IsRunning())
			{
				m_replacing = m_wanted;
				m_replace.BeginOpen("Choose a sound to use instead", kAudioFilter);
			}

			ImGui::CloseCurrentPopup();
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel", Ui::Scaled(110.0f, 0.0f)))
	{
		m_wanted = -1;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void SoundWindow::DrawGetVoice()
{
	if (m_importing && !VoiceImport::IsBusy())
	{
		m_importing = false;
		Reload();
	}

	ImGui::BeginDisabled(VoiceImport::IsBusy());

	if (ImGui::Button("Get this voice from UNI...") && !m_uni.IsRunning())
		m_uni.BeginFolder("Pick the UNDER NIGHT IN-BIRTH folder to take the voice from");

	ImGui::EndDisabled();

	ImGui::SameLine();
	UiText::Help("Point this at the folder holding UNIclr.exe, UNIst.exe or UNIEL.exe and the mod "
		"reads that game's voice for the character above out of your own copy, matching each line "
		"by the text the game itself writes beside it. It lands as a pack of its own, worn by that "
		"character and nobody else, and running it again replaces what it added.");

	if (VoiceImport::IsBusy())
	{
		ImGui::SameLine();
		ImGui::ProgressBar(VoiceImport::Progress() / 100.0f, Ui::Scaled(200.0f, 0.0f));
		UiText::Warn("%s", VoiceImport::StatusText());
		return;
	}

	if (VoiceImport::Progress() == 0)
		return;

	const SoundPacks::Pack* const pack = PackById(VoiceImport::PackId());

	if (pack == nullptr)
	{
		UiText::Muted("%s", VoiceImport::StatusText());
		return;
	}

	if (pack->converting > 0)
	{
		UiText::Warn("converting %d file(s) to Ogg - the voice works once this finishes",
			pack->converting);
		return;
	}

	UiText::Good("%s is in place - leave the match and come back to hear it", pack->name.c_str());
}

void SoundWindow::DrawReplace()
{
	UiText::Muted("Pick a character and the pack it wears, then give any one of its sounds a file "
		"of your own. Back to original puts the game's own sound back.");

	ImGui::Spacing();
	DrawPicker();
	ImGui::Spacing();
	DrawGetVoice();
	ImGui::Spacing();
	DrawNewPack();

	if (CharaSounds::Count() == 0)
	{
		UiText::Muted(CharaSounds::IsLoading() ? "Reading..." : "Nothing loaded yet.");
		return;
	}

	DrawList();
}

void SoundWindow::DrawChooser(const char* label)
{
	const char* const current = SoundPacks::ChoiceFor(m_chara);

	std::vector<const SoundPacks::Pack*> offered;

	for (int i = 0; i < SoundPacks::Count(); ++i)
	{
		const SoundPacks::Pack* const pack = SoundPacks::Get(i);

		if (pack != nullptr && SoundPacks::Covers(*pack, m_chara))
			offered.push_back(pack);
	}

	int at = 0;

	for (size_t i = 0; i < offered.size(); ++i)
	{
		if (offered[i]->id == current)
			at = static_cast<int>(i) + 1;
	}

	int picked = at;

	if (ImGui::BeginCombo(label, LabelFor(current)))
	{
		if (ImGui::Selectable(kGame, at == 0))
			picked = 0;

		for (size_t i = 0; i < offered.size(); ++i)
		{
			if (ImGui::Selectable(offered[i]->name.c_str(), at == static_cast<int>(i) + 1))
				picked = static_cast<int>(i) + 1;
		}

		ImGui::EndCombo();
	}

	WheelPicked(picked, static_cast<int>(offered.size()) + 1);

	if (picked == at)
		return;

	SoundPacks::Choose(m_chara, picked == 0 ? std::string() : offered[picked - 1]->id);
	CharaSounds::Restate();
}

void SoundWindow::DrawPacks()
{
	UiText::Muted("%s", SoundPacks::StatusText());
	UiText::Muted("%s", SoundPacks::Root().c_str());

	ImGui::Spacing();

	if (ImGui::Button("Rescan"))
		SoundPacks::RequestScan();

	ImGui::SameLine();

	if (ImGui::Button("Import a zip...") && !m_import.IsRunning())
		m_import.BeginOpen("Choose a sound pack", kZipFilter);

	ImGui::SameLine();

	if (ImGui::Button("Open the folder"))
	{
		ShellExecuteA(nullptr, "open", SoundPacks::Root().c_str(), nullptr, nullptr,
			SW_SHOWNORMAL);
	}

	ImGui::Spacing();

	if (!ImGui::BeginTable("packs", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		return;

	ImGui::TableSetupColumn("Pack");
	ImGui::TableSetupColumn("From");
	ImGui::TableSetupColumn("Carries");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();

	for (int i = 0; i < SoundPacks::Count(); ++i)
	{
		const SoundPacks::Pack* const pack = SoundPacks::Get(i);

		if (pack == nullptr)
			continue;

		ImGui::PushID(i);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(pack->name.c_str());

		if (!pack->author.empty())
			UiText::Muted("by %s", pack->author.c_str());

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(pack->source.empty() ? "-" : pack->source.c_str());

		ImGui::TableNextColumn();
		ImGui::Text("%d file(s), %d character(s)", pack->files,
			static_cast<int>(pack->characters.size()));

		if (pack->shared)
		{
			bool everyone = _stricmp(SoundPacks::SharedChoice(), pack->id.c_str()) == 0;

			if (ImGui::Checkbox("everyone hears its shared sounds", &everyone))
				SoundPacks::ChooseShared(everyone ? pack->id : std::string());
		}

		if (pack->converting > 0)
			UiText::Muted("converting %d to Ogg...", pack->converting);

		if (pack->rejected > 0)
			UiText::Warn("%d file(s) are not audio this can read", pack->rejected);

		ImGui::TableNextColumn();

		if (ImGui::Button("Export...") && !m_export.IsRunning())
		{
			m_exporting = pack->id;
			m_export.BeginSave("Save the pack", kZipFilter, (pack->id + ".zip").c_str());
		}

		ImGui::PopID();
	}

	ImGui::EndTable();
}

void SoundWindow::DrawHowTo()
{
	UiText::Muted("The Replace tab is the short way: pick a character, load its sounds, press New "
		"on the one you want and choose a file. The first change asks you to name a pack to keep "
		"it in; everything after that goes into the same one.");

	ImGui::Spacing();
	UiText::Muted("The folder layout a hand-made pack uses, what pack.ini holds, which formats "
		"play, and how to send a pack to somebody is written out in full here:");

	ImGui::Spacing();
	ImGui::TextUnformatted(SoundsReadme::Path().c_str());

	ImGui::Spacing();

	if (ImGui::Button("Open the readme"))
	{
		ShellExecuteA(nullptr, "open", SoundsReadme::Path().c_str(), nullptr, nullptr,
			SW_SHOWNORMAL);
	}

	ImGui::SameLine();

	if (ImGui::Button("Open the Sounds folder"))
	{
		ShellExecuteA(nullptr, "open", SoundPacks::Root().c_str(), nullptr, nullptr,
			SW_SHOWNORMAL);
	}

	ImGui::Spacing();
	UiText::Muted("A replaced sound is read when the character next loads it, so a change lands "
		"the next time you enter a match or a menu.");
}

void SoundWindow::PumpDialogs()
{
	std::string picked;

	if (m_import.TakeResult(picked) && !picked.empty())
		SoundPacks::Import(picked, m_status, sizeof(m_status));

	if (m_export.TakeResult(picked) && !picked.empty() && !m_exporting.empty())
	{
		SoundPacks::Export(m_exporting, picked, m_status, sizeof(m_status));
		m_exporting.clear();
	}

	if (m_replace.TakeResult(picked) && m_replacing >= 0)
	{
		if (!picked.empty())
			CharaSounds::Replace(m_replacing, picked, m_status, sizeof(m_status));

		m_replacing = -1;
		m_wanted = -1;
	}

	if (m_uni.TakeResult(picked) && !picked.empty())
	{
		const VoiceImport::Source source = VoiceImport::Detect(picked.c_str());

		if (VoiceImport::IsSupported(source))
			m_importing = VoiceImport::Begin(picked.c_str(), m_chara);
		else
			sprintf_s(m_status, "that folder holds %s", VoiceImport::SourceName(source));
	}
}
