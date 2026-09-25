#include "Overlay/Native/GuideScreen.h"

#include "Core/ThreadRole.h"
#include "D3D9/Draw/BitmapFont.h"
#include "D3D9/Draw/GameFont.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Menus/MenuWords.h"
#include "Overlay/Native/GameArt.h"
#include "Overlay/Native/KeyGlyph.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr float kReferenceWidth = 1280.0f;
constexpr float kReferenceHeight = 720.0f;
constexpr float kCentre = kReferenceWidth * 0.5f;

constexpr float kHeaderY = 9.0f;
constexpr float kTitleY = 29.0f;
constexpr float kTitlePixels = 24.0f;
constexpr float kTitleIconGap = 4.0f;

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

constexpr float kInfoTop = 573.0f;
constexpr float kInfoBottom = 681.0f;
constexpr float kInfoLabelX = 82.0f;
constexpr float kInfoLabelY = 587.0f;
constexpr float kInfoRuleX = 189.0f;
constexpr float kInfoRuleTop = 580.0f;
constexpr float kInfoTextX = 199.0f;
constexpr float kInfoTextTop = 584.0f;
constexpr float kInfoTextRight = 1210.0f;
constexpr float kInfoPixels = 19.0f;
constexpr float kInfoLinePitch = 31.0f;
constexpr int kInfoLines = 3;

constexpr float kPromptTop = 683.0f;
constexpr float kPromptY = 702.0f;
constexpr float kPromptRight = 1215.0f;
constexpr float kPromptKeySize = 26.0f;
constexpr float kPromptPixels = 19.0f;
constexpr float kPromptGap = 6.0f;
constexpr float kPromptGroupGap = 24.0f;

constexpr float kTracking = 1.5f;
constexpr size_t kLineBytes = 256;
constexpr size_t kWordBytes = 96;

constexpr uint32_t kDim = 0x80000000;
constexpr uint32_t kBody = 0xD83A4047;
constexpr uint32_t kHeadingBand = 0xF02C3136;
constexpr uint32_t kGlowEdge = 0x00C81E1E;
constexpr uint32_t kGlowTopCentre = 0x28C81E1E;
constexpr uint32_t kGlowBottomCentre = 0x78C81E1E;
constexpr uint32_t kRuleEdge = 0x70960000;
constexpr uint32_t kRuleCentre = 0xFFE00808;
constexpr uint32_t kInfoPanel = 0xF0222326;
constexpr uint32_t kPromptBar = 0xB0141516;
constexpr uint32_t kPromptEdge = 0x40FFFFFF;
constexpr uint32_t kText = 0xFFFFFFFF;
constexpr uint32_t kMuted = 0xFFB4B8BE;
constexpr uint32_t kHeading = 0xFFFFE800;
constexpr uint32_t kTitleInk = 0xFF141414;
constexpr uint32_t kTitleGlow = 0x70FFFFFF;
constexpr uint32_t kSeparator = 0x2CFFFFFF;
constexpr uint32_t kSelectLeft = 0xF0D40000;
constexpr uint32_t kSelectRight = 0xE0880000;
constexpr uint32_t kSelectEdge = 0x70FF9090;
constexpr uint32_t kSwatchOutline = 0xC0000000;

const char* const kFallbackSelect = "Select";
const char* const kFallbackReturn = "Return to menu";

struct Frame
{
	float scale;
	float left;
	float top;
	float width;
	float height;

	float X(float x) const
	{
		return left + x * scale;
	}

	float Y(float y) const
	{
		return top + y * scale;
	}

	float S(float value) const
	{
		return value * scale;
	}
};

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

float FontScale(const Frame& frame, float pixels)
{
	const float line = GameArt::Font().GetLineHeight();

	return line > 0.0f ? frame.S(pixels) / line : 1.0f;
}

float Width(const Frame& frame, const char* text, float pixels)
{
	return GameArt::Font().MeasureWidth(text, FontScale(frame, pixels), kTracking) / frame.scale;
}

void Text(const Frame& frame, const char* text, float x, float centreY, float pixels, uint32_t colour)
{
	GameArt::Font().Draw(text, frame.X(x), frame.Y(centreY - pixels * 0.5f), FontScale(frame, pixels), colour,
		kTracking);
}

const char* Fit(const Frame& frame, const char* text, float pixels, float width, char* out, size_t size)
{
	if (Width(frame, text, pixels) <= width)
		return text;

	strcpy_s(out, size, text);

	for (size_t length = strlen(out); length > 0; --length)
	{
		strcpy_s(out + length - 1, size - length + 1, "...");

		if (Width(frame, out, pixels) <= width)
			return out;

		out[length - 1] = '\0';
	}

	return "";
}

void Sprite(const Frame& frame, const GameArt::Sprite& sprite, float x, float y, float width, float height)
{
	GameArt::Draw(sprite, frame.X(x), frame.Y(y), frame.S(width), frame.S(height));
}

void Wide(const Frame& frame, const GameArt::Sprite& sprite, float y, float height)
{
	GameArt::Draw(sprite, 0.0f, frame.Y(y), frame.width, frame.S(height));
}

void Word(int index, const char* fallback, char* out)
{
	if (!MenuWords::Copy(index, out, kWordBytes))
		strcpy_s(out, kWordBytes, fallback);
}

void DrawBackdrop(const Frame& frame)
{
	QuadRenderer::FillRect(0.0f, 0.0f, frame.width, frame.height, kDim);
	QuadRenderer::FillRect(0.0f, frame.Y(kHeadingTop), frame.width, frame.S(kBodyTop - kHeadingTop), kHeadingBand);
	QuadRenderer::FillRect(0.0f, frame.Y(kBodyTop), frame.width, frame.S(kInfoTop - kBodyTop), kBody);
	Wide(frame, GameArt::kHeader, kHeaderY, static_cast<float>(GameArt::kHeader.height));
}

void DrawTitle(const Frame& frame, const char* title)
{
	const float iconWidth = static_cast<float>(GameArt::kTitleIcon.width);
	const float iconHeight = static_cast<float>(GameArt::kTitleIcon.height);
	const float textWidth = Width(frame, title, kTitlePixels);
	const float left = kCentre - (iconWidth + kTitleIconGap + textWidth) * 0.5f;
	const float textX = left + iconWidth + kTitleIconGap;

	Sprite(frame, GameArt::kTitleIcon, left, kTitleY - iconHeight * 0.5f, iconWidth, iconHeight);

	for (int dx = -1; dx <= 1; ++dx)
	{
		for (int dy = -1; dy <= 1; ++dy)
		{
			if (dx != 0 || dy != 0)
				Text(frame, title, textX + dx, kTitleY + dy, kTitlePixels, kTitleGlow);
		}
	}

	Text(frame, title, textX, kTitleY, kTitlePixels, kTitleInk);
}

void DrawPageBar(const Frame& frame, const GuideContent& content, int current)
{
	const float bandWidth = static_cast<float>(GameArt::kPageBand.width);
	const float bandHeight = static_cast<float>(GameArt::kPageBand.height);
	const char* const title = content.pages[current].title;

	const float bandRight = frame.X(kBandX + bandWidth);

	GameArt::Draw(GameArt::kPageBandLeft, 0.0f, frame.Y(kBandY), frame.X(kBandX), frame.S(bandHeight));
	Sprite(frame, GameArt::kPageBand, kBandX, kBandY, bandWidth, bandHeight);
	GameArt::Draw(GameArt::kPageBandRight, bandRight, frame.Y(kBandY), frame.width - bandRight,
		frame.S(bandHeight));
	Text(frame, title, kCentre - Width(frame, title, kPagePixels) * 0.5f, kPageTitleY, kPagePixels, kText);

	const float keyY = kPageTitleY - kPageKeySize * 0.5f;
	const float previous = KeyGlyph::Width(GameOffsets::kMenuKeyPreviousPage, kPageKeySize);
	const float next = KeyGlyph::Width(GameOffsets::kMenuKeyNextPage, kPageKeySize);

	KeyGlyph::Draw(GameOffsets::kMenuKeyPreviousPage, frame.X(kPageKeyLeft - previous * 0.5f), frame.Y(keyY),
		frame.S(kPageKeySize));
	KeyGlyph::Draw(GameOffsets::kMenuKeyNextPage, frame.X(kPageKeyRight - next * 0.5f), frame.Y(keyY),
		frame.S(kPageKeySize));

	const float first = kCentre - (content.pageCount - 1) * kDotPitch * 0.5f - kDotSize * 0.5f;

	for (int i = 0; i < content.pageCount; ++i)
	{
		Sprite(frame, i == current ? GameArt::kDotCurrent : GameArt::kDot, first + i * kDotPitch, kDotsTop,
			kDotSize, kDotSize);
	}
}

void DrawHeading(const Frame& frame, const GuidePage& page)
{
	char heading[kLineBytes] = {};
	sprintf_s(heading, "[%s]", page.heading);

	const float half = frame.S((kRuleRight - kRuleLeft) * 0.5f);
	const float glowHeight = frame.S(kRuleY - kGlowTop);

	QuadRenderer::FillRectCorners(frame.X(kRuleLeft), frame.Y(kGlowTop), half, glowHeight, kGlowEdge,
		kGlowTopCentre, kGlowEdge, kGlowBottomCentre);
	QuadRenderer::FillRectCorners(frame.X(kCentre), frame.Y(kGlowTop), half, glowHeight, kGlowTopCentre,
		kGlowEdge, kGlowBottomCentre, kGlowEdge);
	QuadRenderer::FillRectHorizontal(frame.X(kRuleLeft), frame.Y(kRuleY), half, frame.S(kRuleHeight), kRuleEdge,
		kRuleCentre);
	QuadRenderer::FillRectHorizontal(frame.X(kCentre), frame.Y(kRuleY), half, frame.S(kRuleHeight), kRuleCentre,
		kRuleEdge);

	Text(frame, heading, kHeadingX, kHeadingY, kRowPixels, kHeading);
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

	Text(frame, row.name, kNameX, centre, kRowPixels, kText);
	char fitted[kLineBytes] = {};
	const char* const summary = Fit(frame, row.summary, kRowPixels, kScrollX - kSummaryX - kScrollWidth, fitted,
		sizeof(fitted));

	Text(frame, summary, kSummaryX, centre, kRowPixels, selected ? kText : kMuted);
}

void DrawScrollBar(const Frame& frame, int count, int scroll)
{
	if (count <= kVisibleRows)
		return;

	const float track = kVisibleRows * kRowPitch;
	const float thumb = track * kVisibleRows / count;
	const float offset = track * scroll / count;

	Sprite(frame, GameArt::kScrollTrack, kScrollX, kRowsTop, kScrollWidth, track);
	Sprite(frame, GameArt::kScrollThumb, kScrollX + 1.0f, kRowsTop + offset, kScrollWidth - 2.0f, thumb);
}

void DrawRows(const Frame& frame, const GuidePage& page, int selected, int scroll)
{
	const int last = scroll + kVisibleRows < page.rowCount ? scroll + kVisibleRows : page.rowCount;

	for (int i = scroll; i < last; ++i)
		DrawRow(frame, page.rows[i], kRowsTop + (i - scroll) * kRowPitch, i == selected);

	DrawScrollBar(frame, page.rowCount, scroll);
}

void WrappedText(const Frame& frame, const char* text, float x, float top, float right)
{
	char line[kLineBytes] = {};
	char candidate[kLineBytes] = {};
	int lines = 0;

	const auto flush = [&]() {
		Text(frame, line, x, top + kInfoPixels * 0.5f + lines * kInfoLinePitch, kInfoPixels, kText);
		line[0] = '\0';
		++lines;
	};

	for (const char* word = text; *word != '\0' && lines < kInfoLines; )
	{
		if (*word == '\n')
		{
			if (line[0] != '\0')
				flush();

			while (*word == '\n')
				++word;

			continue;
		}

		const char* end = word;
		while (*end != '\0' && *end != ' ' && *end != '\n')
			++end;

		const size_t used = strlen(line);
		const size_t length = static_cast<size_t>(end - word);

		if (used + length + 2 >= kLineBytes)
			break;

		strcpy_s(candidate, line);
		if (used > 0)
			strcat_s(candidate, " ");
		strncat_s(candidate, word, length);

		if (used > 0 && Width(frame, candidate, kInfoPixels) > right - x)
		{
			flush();
			continue;
		}

		strcpy_s(line, candidate);
		word = *end == ' ' ? end + 1 : end;
	}

	if (line[0] != '\0' && lines < kInfoLines)
		flush();
}

void DrawInformation(const Frame& frame, const GuideRow& row)
{
	QuadRenderer::FillRect(0.0f, frame.Y(kInfoTop), frame.width, frame.S(kInfoBottom - kInfoTop), kInfoPanel);

	Sprite(frame, GameArt::kInformation, kInfoLabelX, kInfoLabelY,
		static_cast<float>(GameArt::kInformation.width), static_cast<float>(GameArt::kInformation.height));
	Sprite(frame, GameArt::kInformationRule, kInfoRuleX, kInfoRuleTop,
		static_cast<float>(GameArt::kInformationRule.width), kInfoBottom - kInfoRuleTop - 8.0f);

	WrappedText(frame, row.detail, kInfoTextX, kInfoTextTop, kInfoTextRight);
}

float PromptKeys(const Frame& frame, const int* functions, int count, float right, bool draw)
{
	float width = 0.0f;

	for (int i = count - 1; i >= 0; --i)
	{
		const float key = KeyGlyph::Width(functions[i], kPromptKeySize);

		if (draw)
		{
			KeyGlyph::Draw(functions[i], frame.X(right - width - key), frame.Y(kPromptY - kPromptKeySize * 0.5f),
				frame.S(kPromptKeySize));
		}

		width += key;
	}

	return width;
}

float PromptText(const Frame& frame, const char* text, float right)
{
	const float width = Width(frame, text, kPromptPixels);

	Text(frame, text, right - width, kPromptY, kPromptPixels, kText);
	return width;
}

void DrawPrompt(const Frame& frame)
{
	QuadRenderer::FillRect(0.0f, frame.Y(kPromptTop), frame.width, frame.height - frame.Y(kPromptTop),
		kPromptBar);
	QuadRenderer::FillRect(0.0f, frame.Y(kPromptTop), frame.width, frame.S(1.0f), kPromptEdge);

	char select[kWordBytes] = {};
	char back[kWordBytes] = {};
	Word(GameOffsets::kMenuWordSelect, kFallbackSelect, select);
	Word(GameOffsets::kMenuWordReturnToMenu, kFallbackReturn, back);

	static const int kCancel[] = { GameOffsets::kMenuKeyCancel };
	static const int kConfirm[] = { GameOffsets::kMenuKeyConfirm };
	static const int kMove[] = { GameOffsets::kMenuKeyUp, GameOffsets::kMenuKeyLeft, GameOffsets::kMenuKeyDown,
		GameOffsets::kMenuKeyRight };

	float right = kPromptRight;

	right -= PromptText(frame, back, right) + kPromptGap;
	right -= PromptKeys(frame, kCancel, 1, right, true) + kPromptGap;
	right -= PromptText(frame, "or", right) + kPromptGap;
	right -= PromptKeys(frame, kConfirm, 1, right, true) + kPromptGroupGap;
	right -= PromptText(frame, select, right) + kPromptGap;
	PromptKeys(frame, kMove, 4, right, true);
}

Frame FrameFor(const D3DVIEWPORT9& viewport)
{
	Frame frame = {};
	frame.scale = viewport.Height / kReferenceHeight;
	frame.width = static_cast<float>(viewport.Width);
	frame.height = static_cast<float>(viewport.Height);
	frame.left = viewport.X + (frame.width - kReferenceWidth * frame.scale) * 0.5f;
	frame.top = static_cast<float>(viewport.Y);

	return frame;
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

	if (!m_open.load() || !TrainingMenu::IsActive() || device == nullptr)
		return;

	D3DVIEWPORT9 viewport = {};
	if (FAILED(device->GetViewport(&viewport)) || viewport.Height == 0)
		return;

	GameFont::Ensure(device);
	GameArt::Ensure(device);
	KeyGlyph::Observe();

	const GuideContent& content = m_content();
	const int current = Clamp(m_page.load(), content.pageCount);
	const GuidePage& page = content.pages[current];
	const int row = Clamp(m_row.load(), page.rowCount);
	const Frame frame = FrameFor(viewport);

	DrawBackdrop(frame);
	DrawTitle(frame, content.title);
	DrawPageBar(frame, content, current);
	DrawHeading(frame, page);
	DrawRows(frame, page, row, Clamp(m_scroll.load(), page.rowCount));
	DrawInformation(frame, page.rows[row]);
	DrawPrompt(frame);
}
