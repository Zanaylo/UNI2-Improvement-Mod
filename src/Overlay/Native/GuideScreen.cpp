#include "Overlay/Native/GuideScreen.h"

#include "Core/ThreadRole.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "Game/Engine/GameOffsets.h"
#include "Overlay/Native/GameArt.h"
#include "Overlay/Native/KeyGlyph.h"
#include "Overlay/Native/NativeDraw.h"

#include <cstdio>

namespace {

using NativeDraw::Frame;

constexpr float kHeaderY = 9.0f;
constexpr float kTitleY = 29.0f;

constexpr float kBandX = 424.0f;
constexpr float kBandY = 45.0f;
constexpr float kPageTitleY = 65.0f;
constexpr float kPagePixels = 20.0f;
constexpr float kPageKeyLeft = 445.0f;
constexpr float kPageKeyRight = 817.0f;
constexpr float kPageKeySize = 28.0f;

constexpr float kDotsTop = 79.0f;
constexpr float kDotSize = 16.0f;
constexpr float kDotPitch = 24.0f;

constexpr float kHeadingTop = 94.0f;
constexpr float kHeadingX = 212.0f;
constexpr float kHeadingY = 108.0f;
constexpr float kGlowTop = 92.0f;
constexpr float kRuleLeft = 67.0f;
constexpr float kRuleRight = 1213.0f;
constexpr float kRuleY = 121.0f;
constexpr float kRuleHeight = 3.0f;
constexpr float kBodyTop = 127.0f;

constexpr float kRowsTop = 130.0f;
constexpr float kRowPitch = 46.0f;
constexpr int kVisibleRows = 9;
constexpr float kRowLeft = 150.0f;
constexpr float kRowRight = 1200.0f;
constexpr float kSelectHeight = 34.0f;
constexpr float kSwatchX = 186.0f;
constexpr float kSwatchSize = 16.0f;
constexpr float kNameX = 211.0f;
constexpr float kSummaryX = 720.0f;
constexpr float kRowPixels = 20.0f;

constexpr float kScrollX = 1228.0f;
constexpr float kScrollWidth = 8.0f;

constexpr NativeDraw::InfoBox kInfoBox = { 573.0f, 681.0f };

constexpr float kPromptRight = 1215.0f;
constexpr float kPromptGap = 6.0f;
constexpr float kPromptGroupGap = 24.0f;

constexpr uint32_t kDim = 0x80000000;
constexpr uint32_t kBody = 0xD83A4047;
constexpr uint32_t kHeadingBand = 0xF02C3136;
constexpr uint32_t kGlowEdge = 0x00C81E1E;
constexpr uint32_t kGlowTopCentre = 0x28C81E1E;
constexpr uint32_t kGlowBottomCentre = 0x78C81E1E;
constexpr uint32_t kRuleEdge = 0x70960000;
constexpr uint32_t kRuleCentre = 0xFFE00808;
constexpr uint32_t kText = 0xFFFFFFFF;
constexpr uint32_t kMuted = 0xFFB4B8BE;
constexpr uint32_t kHeading = 0xFFFFE800;
constexpr uint32_t kSeparator = 0x2CFFFFFF;
constexpr uint32_t kSelectLeft = 0xF0D40000;
constexpr uint32_t kSelectRight = 0xE0880000;
constexpr uint32_t kSelectEdge = 0x70FF9090;
constexpr uint32_t kSwatchOutline = 0xC0000000;

const char* const kFallbackSelect = "Select";
const char* const kFallbackReturn = "Return to menu";

int Wrap(int value, int count)
{
	return count <= 0 ? 0 : (value % count + count) % count;
}

int Clamp(int value, int count)
{
	if (value < 0 || count <= 0)
		return 0;

	return value < count ? value : count - 1;
}

void DrawBackdrop(const Frame& frame)
{
	QuadRenderer::FillRect(0.0f, 0.0f, frame.width, frame.height, kDim);
	QuadRenderer::FillRect(0.0f, frame.Y(kHeadingTop), frame.width, frame.S(kBodyTop - kHeadingTop), kHeadingBand);
	QuadRenderer::FillRect(0.0f, frame.Y(kBodyTop), frame.width, frame.S(kInfoBox.top - kBodyTop), kBody);
	NativeDraw::Wide(frame, GameArt::kHeader, kHeaderY, static_cast<float>(GameArt::kHeader.height));
}

void DrawPageBar(const Frame& frame, const GuideContent& content, int current)
{
	const float bandWidth = static_cast<float>(GameArt::kPageBand.width);
	const float bandHeight = static_cast<float>(GameArt::kPageBand.height);
	const char* const title = content.pages[current].title;

	const float bandRight = frame.X(kBandX + bandWidth);

	GameArt::Draw(GameArt::kPageBandLeft, 0.0f, frame.Y(kBandY), frame.X(kBandX), frame.S(bandHeight));
	NativeDraw::Sprite(frame, GameArt::kPageBand, kBandX, kBandY, bandWidth, bandHeight);
	GameArt::Draw(GameArt::kPageBandRight, bandRight, frame.Y(kBandY), frame.width - bandRight,
		frame.S(bandHeight));
	NativeDraw::Text(frame, title, NativeDraw::kCentre - NativeDraw::Width(frame, title, kPagePixels) * 0.5f,
		kPageTitleY, kPagePixels, kText);

	const float keyY = kPageTitleY - kPageKeySize * 0.5f;
	const float previous = KeyGlyph::Width(GameOffsets::kMenuKeyPreviousPage, kPageKeySize);
	const float next = KeyGlyph::Width(GameOffsets::kMenuKeyNextPage, kPageKeySize);

	KeyGlyph::Draw(GameOffsets::kMenuKeyPreviousPage, frame.X(kPageKeyLeft - previous * 0.5f), frame.Y(keyY),
		frame.S(kPageKeySize));
	KeyGlyph::Draw(GameOffsets::kMenuKeyNextPage, frame.X(kPageKeyRight - next * 0.5f), frame.Y(keyY),
		frame.S(kPageKeySize));

	const float first = NativeDraw::kCentre - (content.pageCount - 1) * kDotPitch * 0.5f - kDotSize * 0.5f;

	for (int i = 0; i < content.pageCount; ++i)
	{
		NativeDraw::Sprite(frame, i == current ? GameArt::kDotCurrent : GameArt::kDot, first + i * kDotPitch,
			kDotsTop, kDotSize, kDotSize);
	}
}

void DrawHeading(const Frame& frame, const GuidePage& page)
{
	char heading[NativeDraw::kLineBytes] = {};
	sprintf_s(heading, "[%s]", page.heading);

	const float half = frame.S((kRuleRight - kRuleLeft) * 0.5f);
	const float glowHeight = frame.S(kRuleY - kGlowTop);

	QuadRenderer::FillRectCorners(frame.X(kRuleLeft), frame.Y(kGlowTop), half, glowHeight, kGlowEdge,
		kGlowTopCentre, kGlowEdge, kGlowBottomCentre);
	QuadRenderer::FillRectCorners(frame.X(NativeDraw::kCentre), frame.Y(kGlowTop), half, glowHeight, kGlowTopCentre,
		kGlowEdge, kGlowBottomCentre, kGlowEdge);
	QuadRenderer::FillRectHorizontal(frame.X(kRuleLeft), frame.Y(kRuleY), half, frame.S(kRuleHeight), kRuleEdge,
		kRuleCentre);
	QuadRenderer::FillRectHorizontal(frame.X(NativeDraw::kCentre), frame.Y(kRuleY), half, frame.S(kRuleHeight),
		kRuleCentre, kRuleEdge);

	NativeDraw::Text(frame, heading, kHeadingX, kHeadingY, kRowPixels, kHeading);
}

void DrawRow(const Frame& frame, const GuideRow& row, float top, bool selected)
{
	const float centre = top + kRowPitch * 0.5f;
	const float width = frame.S(kRowRight - kRowLeft);

	if (selected)
	{
		const float barTop = centre - kSelectHeight * 0.5f;

		QuadRenderer::FillRectHorizontal(frame.X(kRowLeft), frame.Y(barTop), width, frame.S(kSelectHeight),
			kSelectLeft, kSelectRight);
		QuadRenderer::FillRect(frame.X(kRowLeft), frame.Y(barTop), width, frame.S(1.0f), kSelectEdge);
	}

	QuadRenderer::FillRect(frame.X(kRowLeft), frame.Y(top + kRowPitch - 1.0f), width, frame.S(1.0f),
		kSeparator);

	if (row.colour != kGuideNoSwatch)
	{
		const float swatchTop = centre - kSwatchSize * 0.5f;

		QuadRenderer::FillRect(frame.X(kSwatchX), frame.Y(swatchTop), frame.S(kSwatchSize),
			frame.S(kSwatchSize), row.colour);
		QuadRenderer::StrokeRect(frame.X(kSwatchX), frame.Y(swatchTop), frame.S(kSwatchSize),
			frame.S(kSwatchSize), frame.S(1.0f), kSwatchOutline);
	}

	NativeDraw::Text(frame, row.name, kNameX, centre, kRowPixels, kText);
	char fitted[NativeDraw::kLineBytes] = {};
	const char* const summary = NativeDraw::Fit(frame, row.summary, kRowPixels, kScrollX - kSummaryX - kScrollWidth,
		fitted, sizeof(fitted));

	NativeDraw::Text(frame, summary, kSummaryX, centre, kRowPixels, selected ? kText : kMuted);
}

void DrawScrollBar(const Frame& frame, int count, int scroll)
{
	if (count <= kVisibleRows)
		return;

	const float track = kVisibleRows * kRowPitch;
	const float thumb = track * kVisibleRows / count;
	const float offset = track * scroll / count;

	NativeDraw::Sprite(frame, GameArt::kScrollTrack, kScrollX, kRowsTop, kScrollWidth, track);
	NativeDraw::Sprite(frame, GameArt::kScrollThumb, kScrollX + 1.0f, kRowsTop + offset, kScrollWidth - 2.0f, thumb);
}

void DrawRows(const Frame& frame, const GuidePage& page, int selected, int scroll)
{
	const int last = scroll + kVisibleRows < page.rowCount ? scroll + kVisibleRows : page.rowCount;

	for (int i = scroll; i < last; ++i)
		DrawRow(frame, page.rows[i], kRowsTop + (i - scroll) * kRowPitch, i == selected);

	DrawScrollBar(frame, page.rowCount, scroll);
}

void DrawPrompt(const Frame& frame)
{
	NativeDraw::PromptBar(frame);

	char select[NativeDraw::kWordBytes] = {};
	char back[NativeDraw::kWordBytes] = {};
	NativeDraw::Word(GameOffsets::kMenuWordSelect, kFallbackSelect, select);
	NativeDraw::Word(GameOffsets::kMenuWordReturnToMenu, kFallbackReturn, back);

	static const int kCancel[] = { GameOffsets::kMenuKeyCancel };
	static const int kConfirm[] = { GameOffsets::kMenuKeyConfirm };
	static const int kMove[] = { GameOffsets::kMenuKeyUp, GameOffsets::kMenuKeyLeft, GameOffsets::kMenuKeyDown,
		GameOffsets::kMenuKeyRight };

	float right = kPromptRight;

	right -= NativeDraw::PromptText(frame, back, right) + kPromptGap;
	right -= NativeDraw::PromptKeys(frame, kCancel, 1, right) + kPromptGap;
	right -= NativeDraw::PromptText(frame, "or", right) + kPromptGap;
	right -= NativeDraw::PromptKeys(frame, kConfirm, 1, right) + kPromptGroupGap;
	right -= NativeDraw::PromptText(frame, select, right) + kPromptGap;
	NativeDraw::PromptKeys(frame, kMove, 4, right);
}

}

GuideScreen::GuideScreen(ContentFn content)
	: m_content(content)
	, m_open(false)
	, m_page(0)
	, m_row(0)
	, m_scroll(0)
{
}

void GuideScreen::Open()
{
	m_open.store(true);
}

bool GuideScreen::IsOpen() const
{
	return m_open.load();
}

void GuideScreen::Close()
{
	m_open.store(false);
}

void GuideScreen::Update(const MenuInput::State& input)
{
	if (input.confirm || input.cancel || input.openMenu)
	{
		Close();
		return;
	}

	if (input.previousPage || input.lever == MenuInput::kLeverLeft)
	{
		TurnPage(-1);
		return;
	}

	if (input.nextPage || input.lever == MenuInput::kLeverRight)
	{
		TurnPage(1);
		return;
	}

	if (input.lever == MenuInput::kLeverUp)
		MoveRow(-1);
	else if (input.lever == MenuInput::kLeverDown)
		MoveRow(1);
}

void GuideScreen::TurnPage(int delta)
{
	m_page.store(Wrap(m_page.load() + delta, m_content().pageCount));
	m_row.store(0);
	m_scroll.store(0);
}

void GuideScreen::MoveRow(int delta)
{
	const GuideContent& content = m_content();
	const int count = content.pages[Clamp(m_page.load(), content.pageCount)].rowCount;
	const int row = Wrap(m_row.load() + delta, count);

	int scroll = m_scroll.load();

	if (row < scroll)
		scroll = row;
	else if (row >= scroll + kVisibleRows)
		scroll = row - kVisibleRows + 1;

	m_scroll.store(scroll);
	m_row.store(row);
}

void GuideScreen::Render(IDirect3DDevice9* device) const
{
	EXPECT_THREAD(ThreadRole::Role_Render);

	Frame frame = {};

	if (!m_open.load() || !TrainingMenu::IsActive() || !NativeDraw::Begin(device, frame))
		return;

	const GuideContent& content = m_content();
	const int current = Clamp(m_page.load(), content.pageCount);
	const GuidePage& page = content.pages[current];
	const int row = Clamp(m_row.load(), page.rowCount);

	DrawBackdrop(frame);
	NativeDraw::Title(frame, content.title, kTitleY, GameArt::kTitleIcon);
	DrawPageBar(frame, content, current);
	DrawHeading(frame, page);
	DrawRows(frame, page, row, Clamp(m_scroll.load(), page.rowCount));
	NativeDraw::Information(frame, page.rows[row].detail, kInfoBox);
	DrawPrompt(frame);
}
