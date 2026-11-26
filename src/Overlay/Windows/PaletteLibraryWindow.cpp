#include "Overlay/Windows/PaletteLibraryWindow.h"

#include "Core/Text/Language.h"
#include "Game/Engine/GameState.h"
#include "Game/Tables/CharaTables.h"
#include "Overlay/Framework/WindowManager.h"
#include "Overlay/Widgets/ColourSlotCombo.h"
#include "Overlay/Widgets/UiScale.h"
#include "Palette/ColourSlots.h"
#include "Palette/PaletteChoice.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteSearch.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace {

constexpr float kWindowWidth = 860.0f;
constexpr float kWindowHeight = 640.0f;
constexpr float kSmallestWidth = 620.0f;
constexpr float kSmallestHeight = 360.0f;
constexpr float kDetailsWidth = 300.0f;
constexpr float kFilterWidth = 220.0f;
constexpr float kOrderWidth = 175.0f;
constexpr float kCardWidth = 104.0f;
constexpr float kCardPicture = 96.0f;
constexpr float kCardInset = 4.0f;
constexpr float kRounding = 3.0f;
constexpr float kBadgePadding = 3.0f;
constexpr float kBadgeMargin = 2.0f;
constexpr float kWornBorder = 2.0f;

constexpr const char* kPaletteDragPayload = "UNI2_PALETTE_CARD";

constexpr ImU32 kPictureBackdrop = IM_COL32(0x18, 0x18, 0x1c, 255);
constexpr ImU32 kBadgeBackdrop = IM_COL32(0x19, 0x19, 0x19, 0xd2);
constexpr ImU32 kBadgeText = IM_COL32(0xff, 0xcc, 0x33, 255);
constexpr ImU32 kExtendedBadgeText = IM_COL32(0x6e, 0xe7, 0xb7, 255);
constexpr ImU32 kWornOutline = IM_COL32(0x66, 0xdd, 0x88, 255);
constexpr ImU32 kDropOutline = IM_COL32(0x34, 0xd3, 0x99, 255);

struct DragPalettePayload
{
	int chara;
	char file[256];
};

const char* OrderLabel(ColourSlots::ExtendedOrder order)
{
	switch (order)
	{
	case ColourSlots::ExtendedOrder::NewestFirst:
		return Tr("Newest first");
	case ColourSlots::ExtendedOrder::ByName:
		return Tr("By name");
	case ColourSlots::ExtendedOrder::Custom:
		return Tr("Custom list");
	default:
		return Tr("Oldest first");
	}
}

float Half(float value)
{
	return value * 0.5f;
}

void ColourBadge(int colour, char* out, size_t size)
{
	if (colour < ColourSlots::kStockColours)
		sprintf_s(out, size, "%03d", colour + 1);
	else
		sprintf_s(out, size, "C%d", colour - ColourSlots::kStockColours + 1);
}

std::vector<PaletteCatalog::Folder> AllFolders()
{
	std::vector<PaletteCatalog::Folder> folders;

	for (int chara = 0; chara < CharaTables::GetCharaCount(); ++chara)
		folders.push_back({ chara, PaletteManager::GetCharaName(chara), PaletteLibrary::FolderFor(chara) });

	return folders;
}

}

PaletteLibraryWindow::PaletteLibraryWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
	, m_scanJob("palette library")
{
}

void PaletteLibraryWindow::BeforeDraw()
{
	ImGui::SetNextWindowSizeConstraints(Ui::Scaled(kSmallestWidth, kSmallestHeight), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::SetNextWindowSize(Ui::Scaled(kWindowWidth, kWindowHeight), ImGuiCond_FirstUseEver);
}

void PaletteLibraryWindow::OnClosed()
{
	m_wantsScan = false;
	m_scanStarted = false;

	m_thumbnails.Release();

	m_selectedCharacter = nullptr;
	m_selected = nullptr;

	m_characters.clear();
	m_characters.shrink_to_fit();
	m_matches.clear();
	m_matches.shrink_to_fit();
}

void PaletteLibraryWindow::StartScan()
{
	std::vector<PaletteCatalog::Folder> folders = AllFolders();

	m_wantsScan = true;

	m_scanStarted = m_scanJob.Start([this, folders = std::move(folders)]()
	{
		std::vector<PaletteCatalog::Character> characters = PaletteCatalog::Read(folders);

		if (!m_wantsScan)
			return false;

		m_scanned = std::move(characters);
		return true;
	});
}

void PaletteLibraryWindow::TakeScan()
{
	if (!m_scanJob.ConsumeFinished())
		return;

	m_thumbnails.ForgetPalettes();

	m_characters = std::move(m_scanned);
	m_scanned.clear();

	SortAllCharacters();
}

void PaletteLibraryWindow::SortCharacter(PaletteCatalog::Character& character)
{
	const ColourSlots::ExtendedOrder mode = ColourSlots::GetExtendedOrder();
	const int chara = character.chara;

	std::stable_sort(character.palettes.begin(), character.palettes.end(),
		[chara, mode](const PaletteCatalog::Palette& a, const PaletteCatalog::Palette& b)
	{
		if (mode == ColourSlots::ExtendedOrder::Custom)
		{
			const int rankA = ColourSlots::CustomOrderRank(chara, a.file.c_str());
			const int rankB = ColourSlots::CustomOrderRank(chara, b.file.c_str());

			if (rankA != rankB)
			{
				if (rankA < 0)
					return false;
				if (rankB < 0)
					return true;
				return rankA < rankB;
			}
		}

		if (mode == ColourSlots::ExtendedOrder::ByName)
			return _stricmp(a.file.c_str(), b.file.c_str()) < 0;

		if (a.created != b.created)
		{
			return mode == ColourSlots::ExtendedOrder::NewestFirst
				? a.created > b.created
				: a.created < b.created;
		}

		return _stricmp(a.file.c_str(), b.file.c_str()) < 0;
	});
}

void PaletteLibraryWindow::SortAllCharacters()
{
	for (PaletteCatalog::Character& character : m_characters)
		SortCharacter(character);

	FindSelected();
}

void PaletteLibraryWindow::ApplyPendingMove()
{
	if (m_pendingMoveChara < 0 || m_pendingMoveFrom.empty() || m_pendingMoveTo.empty())
		return;

	const int chara = m_pendingMoveChara;
	const std::string fromFile = std::move(m_pendingMoveFrom);
	const std::string toFile = std::move(m_pendingMoveTo);

	m_pendingMoveChara = -1;
	m_pendingMoveFrom.clear();
	m_pendingMoveTo.clear();

	for (PaletteCatalog::Character& character : m_characters)
	{
		if (character.chara != chara)
			continue;

		int fromIndex = -1;
		int toIndex = -1;

		for (int i = 0; i < static_cast<int>(character.palettes.size()); ++i)
		{
			if (_stricmp(character.palettes[i].file.c_str(), fromFile.c_str()) == 0)
				fromIndex = i;
			if (_stricmp(character.palettes[i].file.c_str(), toFile.c_str()) == 0)
				toIndex = i;
		}

		if (fromIndex < 0 || toIndex < 0 || fromIndex == toIndex)
			return;

		PaletteCatalog::Palette moved = std::move(character.palettes[fromIndex]);
		character.palettes.erase(character.palettes.begin() + fromIndex);
		character.palettes.insert(character.palettes.begin() + toIndex, std::move(moved));

		std::vector<std::string> customFiles;
		customFiles.reserve(character.palettes.size());

		for (const PaletteCatalog::Palette& palette : character.palettes)
			customFiles.push_back(palette.file);

		ColourSlots::SetCustomOrder(chara, customFiles);
		ColourSlots::SetExtendedOrder(ColourSlots::ExtendedOrder::Custom);
		SortAllCharacters();
		return;
	}
}

void PaletteLibraryWindow::FindSelected()
{
	m_selectedCharacter = nullptr;
	m_selected = nullptr;

	for (const PaletteCatalog::Character& character : m_characters)
	{
		if (character.chara != m_selectedChara)
			continue;

		for (const PaletteCatalog::Palette& palette : character.palettes)
		{
			if (palette.file != m_selectedFile)
				continue;

			m_selectedCharacter = &character;
			m_selected = &palette;
			return;
		}
	}
}

void PaletteLibraryWindow::ReadWorn()
{
	for (int player = 0; player < kPlayers; ++player)
	{
		m_wornChara[player] = PaletteMemory::GetCharaNumber(player);
		m_wornFile[player] = PaletteChoice::WornFile(player);
	}
}

bool PaletteLibraryWindow::IsWorn(int chara, const char* file) const
{
	for (int player = 0; player < kPlayers; ++player)
	{
		if (m_wornChara[player] == chara && _stricmp(m_wornFile[player], file) == 0)
			return true;
	}

	return false;
}

void PaletteLibraryWindow::Draw()
{
	if (!m_scanStarted && !m_scanJob.IsBusy())
		StartScan();

	TakeScan();
	ReadWorn();

	m_thumbnails.BeginFrame();

	DrawToolbar();

	const float detailsWidth = Ui::Scaled(kDetailsWidth);

	ImGui::BeginChild("cards", ImVec2(-detailsWidth - ImGui::GetStyle().ItemSpacing.x, 0.0f), ImGuiChildFlags_Borders);
	DrawCharacters();
	ImGui::EndChild();

	ApplyPendingMove();

	ImGui::SameLine();

	ImGui::BeginChild("details", ImVec2(detailsWidth, 0.0f), ImGuiChildFlags_Borders);
	DrawDetails();
	ImGui::EndChild();
}

void PaletteLibraryWindow::DrawToolbar()
{
	Ui::SetItemWidth(kFilterWidth);
	ImGui::InputTextWithHint("##filter", Tr("Search palettes or characters"), m_filter, sizeof(m_filter));

	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(Tr("EX order"));

	ImGui::SameLine();
	Ui::SetItemWidth(kOrderWidth);

	const ColourSlots::ExtendedOrder currentOrder = ColourSlots::GetExtendedOrder();

	if (ImGui::BeginCombo("##ex-order", OrderLabel(currentOrder)))
	{
		constexpr ColourSlots::ExtendedOrder kOrders[] = {
			ColourSlots::ExtendedOrder::OldestFirst,
			ColourSlots::ExtendedOrder::NewestFirst,
			ColourSlots::ExtendedOrder::ByName,
			ColourSlots::ExtendedOrder::Custom,
		};

		for (ColourSlots::ExtendedOrder option : kOrders)
		{
			const bool selected = option == currentOrder;

			if (ImGui::Selectable(OrderLabel(option), selected))
			{
				ColourSlots::SetExtendedOrder(option);
				SortAllCharacters();
			}

			if (selected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", Tr("Order of extended (EX) palettes on character select.\n"
			"Drag any palette card onto another to arrange them in a custom list."));
	}

	ImGui::SameLine();

	ImGui::BeginDisabled(m_scanJob.IsBusy());

	if (ImGui::Button(TrLabel("Rescan")))
	{
		PaletteLibrary::Forget();
		StartScan();
	}

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", Tr("Reads the palettes folder again, for palettes added while the game runs."));
}

void PaletteLibraryWindow::DrawCharacters()
{
	if (m_characters.empty())
	{
		ImGui::TextWrapped("%s", m_scanJob.IsBusy()
			? Tr("Reading the palettes folder...")
			: Tr("No palettes yet. Save one in the editor, or put .pal files in UNI2-IM\\Palettes\\<character>."));
		return;
	}

	for (const PaletteCatalog::Character& character : m_characters)
		DrawCharacter(character);
}

void PaletteLibraryWindow::CollectMatches(const PaletteCatalog::Character& character)
{
	m_matches.clear();

	const bool everyPalette = PaletteSearch::Matches(character.name.c_str(), m_filter);

	for (const PaletteCatalog::Palette& palette : character.palettes)
	{
		if (everyPalette || PaletteSearch::Matches(palette.title.c_str(), m_filter))
			m_matches.push_back(&palette);
	}
}

void PaletteLibraryWindow::DrawCharacter(const PaletteCatalog::Character& character)
{
	CollectMatches(character);

	if (m_matches.empty())
		return;

	const bool filtering = m_filter[0] != '\0';

	if (filtering)
		ImGui::SetNextItemOpen(true);

	const bool playing = m_wornChara[0] == character.chara || m_wornChara[1] == character.chara;

	char label[96] = {};
	sprintf_s(label, "%s (%d)###%s", character.name.c_str(), static_cast<int>(character.palettes.size()),
		character.name.c_str());

	if (!ImGui::CollapsingHeader(label, playing ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
		return;

	ImGui::PushID(character.chara);
	DrawCards(character);
	ImGui::PopID();
	ImGui::Spacing();
}

void PaletteLibraryWindow::DrawCards(const PaletteCatalog::Character& character)
{
	const ImGuiStyle& style = ImGui::GetStyle();

	const float width = Ui::Scaled(kCardWidth);
	const float height = Ui::Scaled(kCardPicture) + Ui::Scaled(kCardInset) * 2.0f + ImGui::GetTextLineHeightWithSpacing();
	const ImVec2 cell(width, height);

	const float available = ImGui::GetContentRegionAvail().x;
	const int fits = static_cast<int>((available + style.ItemSpacing.x) / (width + style.ItemSpacing.x));
	const int columns = fits > 0 ? fits : 1;
	const int count = static_cast<int>(m_matches.size());

	for (int first = 0; first < count; first += columns)
	{
		const ImVec2 start = ImGui::GetCursorScreenPos();

		if (!ImGui::IsRectVisible(start, ImVec2(start.x + available, start.y + height)))
		{
			ImGui::Dummy(ImVec2(1.0f, height));
			continue;
		}

		DrawRow(character, first, first + columns < count ? first + columns : count, cell);
	}
}

void PaletteLibraryWindow::DrawRow(const PaletteCatalog::Character& character, int first, int end, const ImVec2& cell)
{
	for (int index = first; index < end; ++index)
	{
		if (index > first)
			ImGui::SameLine();

		ImGui::PushID(index);
		DrawCard(character, *m_matches[index], cell);
		ImGui::PopID();
	}
}

void PaletteLibraryWindow::DrawCard(const PaletteCatalog::Character& character, const PaletteCatalog::Palette& palette,
	const ImVec2& cell)
{
	const ImVec2 corner = ImGui::GetCursorScreenPos();
	const ImVec2 end(corner.x + cell.x, corner.y + cell.y);

	if (ImGui::InvisibleButton("card", cell))
	{
		m_selectedChara = character.chara;
		m_selectedFile = palette.file;
		m_selectedCharacter = &character;
		m_selected = &palette;
	}

	bool dropHighlight = false;

	if (ImGui::BeginDragDropSource())
	{
		DragPalettePayload payload = {};
		payload.chara = character.chara;
		strncpy_s(payload.file, palette.file.c_str(), _TRUNCATE);

		ImGui::SetDragDropPayload(kPaletteDragPayload, &payload, sizeof(payload));
		ImGui::TextUnformatted(palette.title.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* const peek = ImGui::GetDragDropPayload();

		if (peek != nullptr && peek->IsDataType(kPaletteDragPayload) && peek->DataSize == sizeof(DragPalettePayload))
		{
			const auto* const incoming = static_cast<const DragPalettePayload*>(peek->Data);

			if (incoming->chara == character.chara && _stricmp(incoming->file, palette.file.c_str()) != 0)
			{
				dropHighlight = true;

				if (ImGui::AcceptDragDropPayload(kPaletteDragPayload) != nullptr)
				{
					m_pendingMoveChara = character.chara;
					m_pendingMoveFrom = incoming->file;
					m_pendingMoveTo = palette.file;
				}
			}
		}

		ImGui::EndDragDropTarget();
	}

	const bool hovered = ImGui::IsItemHovered();
	const bool selected = m_selected == &palette;

	if (hovered && ImGui::GetDragDropPayload() == nullptr)
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(palette.title.c_str());
		ImGui::TextDisabled("%s", Tr("Drag to reorder EX slots"));
		ImGui::EndTooltip();
	}

	ImDrawList* const draw = ImGui::GetWindowDrawList();

	if (selected || hovered)
		draw->AddRectFilled(corner, end, ImGui::GetColorU32(selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered), kRounding);

	if (dropHighlight)
		draw->AddRect(corner, end, kDropOutline, kRounding, kWornBorder);

	const float inset = Ui::Scaled(kCardInset);
	const float side = Ui::Scaled(kCardPicture);
	const ImVec2 picture(corner.x + inset, corner.y + inset);
	const ImVec2 pictureEnd(picture.x + side, picture.y + side);

	draw->AddRectFilled(picture, pictureEnd, kPictureBackdrop);
	m_thumbnails.DrawCard(draw, character.chara, palette.colours.data(), picture, ImVec2(side, side));

	if (IsWorn(character.chara, palette.file.c_str()))
		draw->AddRect(picture, pictureEnd, kWornOutline, 0.0f, kWornBorder);

	DrawBadge(draw, character.chara, palette.file.c_str(), picture, side);

	const char* const name = palette.title.c_str();
	const ImVec2 nameSize = ImGui::CalcTextSize(name);
	const float nameLeft = nameSize.x < cell.x ? corner.x + Half(cell.x - nameSize.x) : corner.x;
	const float nameTop = pictureEnd.y + inset;

	draw->PushClipRect(ImVec2(corner.x, nameTop), end, true);
	draw->AddText(ImVec2(nameLeft, nameTop), ImGui::GetColorU32(ImGuiCol_Text), name);
	draw->PopClipRect();
}

void PaletteLibraryWindow::DrawBadge(ImDrawList* draw, int chara, const char* file, const ImVec2& corner, float width) const
{
	const int colour = ColourSlots::ColourOf(chara, file);

	char text[12] = {};
	ImU32 textColor = kBadgeText;

	if (colour != ColourSlots::kNoColour)
	{
		ColourBadge(colour, text, sizeof(text));
	}
	else
	{
		const int extended = ColourSlots::ExtendedOf(chara, file);

		if (extended == ColourSlots::kNoExtended)
			return;

		sprintf_s(text, sizeof(text), "EX%d", extended);
		textColor = kExtendedBadgeText;
	}

	const ImVec2 size = ImGui::CalcTextSize(text);
	const ImVec2 start(corner.x + width - size.x - kBadgePadding * 2.0f - kBadgeMargin, corner.y + kBadgeMargin);
	const ImVec2 end(start.x + size.x + kBadgePadding * 2.0f, start.y + size.y);

	draw->AddRectFilled(start, end, kBadgeBackdrop, kRounding);
	draw->AddText(ImVec2(start.x + kBadgePadding, start.y), textColor, text);
}

void PaletteLibraryWindow::DrawDetails()
{
	if (m_selected == nullptr || m_selectedCharacter == nullptr)
	{
		ImGui::TextWrapped("%s", Tr("Pick a palette to see it here."));
		return;
	}

	const PaletteCatalog::Character& character = *m_selectedCharacter;
	const PaletteCatalog::Palette& palette = *m_selected;

	ImGui::TextDisabled("%s", character.name.c_str());
	ImGui::TextWrapped("%s", palette.title.c_str());

	if (palette.info.creator[0] != '\0')
		ImGui::TextDisabled(Tr("by %s"), palette.info.creator);

	if (palette.info.description[0] != '\0')
		ImGui::TextWrapped("%s", palette.info.description);

	ImGui::Spacing();

	if (!m_thumbnails.DrawSheet(character.chara, palette.colours.data(), ImGui::GetContentRegionAvail().x))
		ImGui::TextDisabled("%s", Tr("Reading the picture..."));

	ImGui::Spacing();

	ImGui::TextUnformatted(Tr("Colour slot"));
	ColourSlotCombo::Draw(character.chara, palette.file.c_str());

	ImGui::Spacing();

	DrawWearButtons(character, palette);
}

void PaletteLibraryWindow::DrawWearButtons(const PaletteCatalog::Character& character, const PaletteCatalog::Palette& palette)
{
	if (!GameState::AllowsPalettes())
	{
		ImGui::TextDisabled("%s", Tr("During a match, you can wear it from here."));
		return;
	}

	bool any = false;

	for (int player = 0; player < kPlayers; ++player)
	{
		if (m_wornChara[player] != character.chara)
			continue;

		any = true;

		ImGui::PushID(player);
		ImGui::BeginDisabled(!PaletteControl::CanEdit(player));

		char label[48] = {};
		sprintf_s(label, Tr("Wear on P%d"), player + 1);

		if (ImGui::Button(label))
			PaletteChoice::Wear(player, palette.file.c_str());

		ImGui::SameLine();

		sprintf_s(label, Tr("Edit on P%d"), player + 1);

		if (ImGui::Button(label) && PaletteChoice::Wear(player, palette.file.c_str()))
		{
			WindowContainer* const container = WindowManager::GetInstance().GetContainer();
			IWindow* const editor = container != nullptr ? container->GetWindow(WindowType_Palette) : nullptr;

			if (editor != nullptr)
				editor->Open();
		}

		ImGui::EndDisabled();
		ImGui::PopID();
	}

	if (!any)
		ImGui::TextDisabled("%s", Tr("This character is not in the match."));
}
