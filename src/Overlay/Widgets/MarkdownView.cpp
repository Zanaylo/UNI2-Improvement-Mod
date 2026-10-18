#include "Overlay/Widgets/MarkdownView.h"

#include "Core/ShellOpen.h"
#include "Overlay/Widgets/BoldText.h"

#include <imgui.h>

#include <algorithm>

namespace {

using Markdown::Block;
using Markdown::Kind;
using Markdown::Piece;
using Markdown::Style;

const ImVec4 kHeadingColour(0.98f, 0.98f, 1.00f, 1.00f);
const ImVec4 kLinkColour(0.35f, 0.62f, 0.98f, 1.00f);
const ImVec4 kQuoteColour(0.62f, 0.66f, 0.72f, 1.00f);
const ImVec4 kCodeColour(0.90f, 0.80f, 0.62f, 1.00f);
const ImU32 kCodeChipColour = IM_COL32(60, 66, 78, 110);
const ImU32 kCodeBlockColour = IM_COL32(26, 28, 33, 200);
const ImU32 kQuoteBarColour = IM_COL32(96, 102, 112, 255);

constexpr float kHeadingScales[] = { 1.70f, 1.45f, 1.22f, 1.05f };
constexpr int kRuledHeadings = 2;
constexpr float kChipPadding = 3.0f;
constexpr float kListIndent = 8.0f;
constexpr float kListLevelIndent = 14.0f;
constexpr float kQuoteIndent = 10.0f;
constexpr float kQuoteBarWidth = 3.0f;

bool g_bold = false;

Style Effective(Style style)
{
	return g_bold && style == Style::Normal ? Style::Bold : style;
}

float Width(const Piece& piece)
{
	const Style style = Effective(piece.style);

	if (style == Style::Bold)
		return BoldText::Size(piece.text.c_str()).x;

	const float text = ImGui::CalcTextSize(piece.text.c_str()).x;

	if (style == Style::Code)
		return text + kChipPadding * 2.0f;

	return text;
}

void DrawCode(const Piece& piece)
{
	const ImVec2 at = ImGui::GetCursorScreenPos();
	const ImVec2 size = ImGui::CalcTextSize(piece.text.c_str());

	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(at.x, at.y - 1.0f),
		ImVec2(at.x + size.x + kChipPadding * 2.0f, at.y + size.y + 1.0f), kCodeChipColour, kChipPadding);

	ImGui::Dummy(ImVec2(kChipPadding, size.y));
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, kCodeColour);
	ImGui::TextUnformatted(piece.text.c_str());
	ImGui::PopStyleColor();
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::Dummy(ImVec2(kChipPadding, size.y));
}

void DrawLink(const Piece& piece)
{
	ImGui::PushStyleColor(ImGuiCol_Text, kLinkColour);
	ImGui::TextUnformatted(piece.text.c_str());
	ImGui::PopStyleColor();

	const ImVec2 low = ImGui::GetItemRectMin();
	const ImVec2 high = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddLine(ImVec2(low.x, high.y - 1.0f), ImVec2(high.x, high.y - 1.0f),
		ImGui::GetColorU32(kLinkColour));

	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		ImGui::SetTooltip("%s", piece.url.c_str());
	}

	if (ImGui::IsItemClicked())
		ShellOpen::Open(piece.url);
}

void DrawBold(const Piece& piece)
{
	ImGui::TextUnformatted(piece.text.c_str());
	BoldText::Draw(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetColorU32(ImGuiCol_Text),
		piece.text.c_str());
}

void DrawWords(const Piece& piece)
{
	if (Effective(piece.style) == Style::Bold)
	{
		DrawBold(piece);
		return;
	}

	ImGui::TextUnformatted(piece.text.c_str());

	if (piece.style != Style::Strike)
		return;

	const ImVec2 low = ImGui::GetItemRectMin();
	const ImVec2 high = ImGui::GetItemRectMax();
	const float middle = low.y + (high.y - low.y) * 0.5f;
	ImGui::GetWindowDrawList()->AddLine(ImVec2(low.x, middle), ImVec2(high.x, middle),
		ImGui::GetColorU32(ImGuiCol_Text));
}

void DrawPiece(const Piece& piece)
{
	if (piece.style == Style::Code)
	{
		DrawCode(piece);
		return;
	}

	if (piece.style == Style::Link)
	{
		DrawLink(piece);
		return;
	}

	DrawWords(piece);
}

void DrawInline(const std::vector<Piece>& pieces)
{
	if (pieces.empty())
	{
		ImGui::NewLine();
		return;
	}

	const float space = ImGui::CalcTextSize(" ").x;
	const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
	bool first = true;

	for (const Piece& piece : pieces)
	{
		const float gap = piece.spaceBefore ? space : 0.0f;

		if (!first && ImGui::GetItemRectMax().x + gap + Width(piece) <= right)
			ImGui::SameLine(0.0f, gap);

		DrawPiece(piece);
		first = false;
	}
}

float HeadingScale(int level)
{
	const int last = static_cast<int>(sizeof(kHeadingScales) / sizeof(kHeadingScales[0])) - 1;

	return kHeadingScales[(std::min)(level - 1, last)];
}

void DrawHeading(const Block& block)
{
	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Text, kHeadingColour);
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * HeadingScale(block.level));
	g_bold = true;
	DrawInline(block.pieces);
	g_bold = false;
	ImGui::PopFont();
	ImGui::PopStyleColor();

	if (block.level <= kRuledHeadings)
		ImGui::Separator();

	ImGui::Spacing();
}

void DrawListItem(const Block& block)
{
	const float indent = kListIndent + block.level * kListLevelIndent;
	ImGui::Indent(indent);

	if (block.kind == Kind::Bullet && block.marker.empty())
		ImGui::Bullet();
	else
		ImGui::TextUnformatted(block.marker.c_str());

	ImGui::SameLine();
	DrawInline(block.pieces);
	ImGui::Unindent(indent);
}

void DrawQuote(const Block& block)
{
	const ImVec2 start = ImGui::GetCursorScreenPos();

	ImGui::Indent(kQuoteIndent);
	ImGui::PushStyleColor(ImGuiCol_Text, kQuoteColour);
	DrawInline(block.pieces);
	ImGui::PopStyleColor();
	ImGui::Unindent(kQuoteIndent);

	const ImVec2 end = ImGui::GetCursorScreenPos();
	ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + kQuoteBarWidth, end.y - 2.0f),
		kQuoteBarColour, 1.0f);
}

void DrawCodeLine(const Block& block)
{
	const char* const text = block.pieces.empty() ? "" : block.pieces.front().text.c_str();
	const ImVec2 at = ImGui::GetCursorScreenPos();
	const float height = ImGui::GetTextLineHeight();

	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(at.x - 2.0f, at.y - 1.0f),
		ImVec2(at.x + ImGui::GetContentRegionAvail().x, at.y + height + 1.0f), kCodeBlockColour);

	ImGui::PushStyleColor(ImGuiCol_Text, kCodeColour);
	ImGui::TextUnformatted(text);
	ImGui::PopStyleColor();
}

void DrawBlock(const Block& block)
{
	switch (block.kind)
	{
	case Kind::Heading:
		DrawHeading(block);
		return;
	case Kind::Bullet:
	case Kind::Numbered:
		DrawListItem(block);
		return;
	case Kind::Quote:
		DrawQuote(block);
		return;
	case Kind::Code:
		DrawCodeLine(block);
		return;
	case Kind::Rule:
		ImGui::Separator();
		return;
	case Kind::Gap:
		ImGui::Spacing();
		return;
	case Kind::Paragraph:
		DrawInline(block.pieces);
		return;
	}
}

}

void MarkdownView::Draw(const std::vector<Markdown::Block>& blocks)
{
	for (const Block& block : blocks)
		DrawBlock(block);
}
