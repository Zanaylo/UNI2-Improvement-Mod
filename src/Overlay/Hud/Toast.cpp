#include "Overlay/Hud/Toast.h"

#include "Core/Text/Language.h"
#include "Overlay/Widgets/UiScale.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdarg>
#include <cstdio>

namespace {

constexpr float kTopFraction = 0.12f;
constexpr float kWidth = 360.0f;
constexpr float kMargin = 24.0f;
constexpr float kPadding = 12.0f;
constexpr float kStripe = 4.0f;
constexpr float kIconRadius = 11.0f;
constexpr float kIconStroke = 2.2f;
constexpr float kGap = 10.0f;
constexpr float kRounding = 8.0f;
constexpr float kSlide = 28.0f;
constexpr float kShadowDrop = 4.0f;
constexpr float kTitleScale = 0.8f;
constexpr float kLineGap = 3.0f;
constexpr float kBarHeight = 3.0f;
constexpr float kBarAlpha = 0.7f;
constexpr float kGlyphScale = 0.45f;

const ImVec4 kCard(0.09f, 0.09f, 0.11f, 0.96f);
const ImVec4 kEdge(1.00f, 1.00f, 1.00f, 0.08f);
const ImVec4 kShadow(0.00f, 0.00f, 0.00f, 0.40f);
const ImVec4 kText(0.96f, 0.96f, 0.98f, 1.00f);
const ImVec4 kInfo(0.30f, 0.62f, 0.95f, 1.00f);
const ImVec4 kSuccess(0.27f, 0.75f, 0.42f, 1.00f);
const ImVec4 kWarning(0.96f, 0.68f, 0.20f, 1.00f);
const ImVec4 kError(0.86f, 0.20f, 0.25f, 1.00f);

ToastQueue g_queue;

ImU32 Faded(const ImVec4& colour, float alpha)
{
	return ImGui::GetColorU32(ImVec4(colour.x, colour.y, colour.z, colour.w * alpha));
}

const ImVec4& AccentOf(ToastKind kind)
{
	switch (kind)
	{
	case ToastKind::Success:
		return kSuccess;
	case ToastKind::Warning:
		return kWarning;
	case ToastKind::Error:
		return kError;
	default:
		return kInfo;
	}
}

void DrawCheck(ImDrawList* draw, const ImVec2& c, float r, ImU32 colour, float stroke)
{
	const ImVec2 corner(c.x - r * 0.25f, c.y + r * 0.75f);

	draw->AddLine(ImVec2(c.x - r, c.y), corner, colour, stroke);
	draw->AddLine(corner, ImVec2(c.x + r, c.y - r * 0.7f), colour, stroke);
}

void DrawCross(ImDrawList* draw, const ImVec2& c, float r, ImU32 colour, float stroke)
{
	draw->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), colour, stroke);
	draw->AddLine(ImVec2(c.x + r, c.y - r), ImVec2(c.x - r, c.y + r), colour, stroke);
}

void DrawBang(ImDrawList* draw, const ImVec2& c, float r, ImU32 colour, float stroke)
{
	draw->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r * 0.3f), colour, stroke);
	draw->AddCircleFilled(ImVec2(c.x, c.y + r * 0.85f), stroke * 0.6f, colour);
}

void DrawInfo(ImDrawList* draw, const ImVec2& c, float r, ImU32 colour, float stroke)
{
	draw->AddCircleFilled(ImVec2(c.x, c.y - r * 0.8f), stroke * 0.6f, colour);
	draw->AddLine(ImVec2(c.x, c.y - r * 0.2f), ImVec2(c.x, c.y + r), colour, stroke);
}

void DrawGlyph(ImDrawList* draw, const ImVec2& centre, float radius, ToastKind kind, ImU32 colour)
{
	const float stroke = Ui::Scaled(kIconStroke);
	const float r = radius * kGlyphScale;

	switch (kind)
	{
	case ToastKind::Success:
		DrawCheck(draw, centre, r, colour, stroke);
		return;
	case ToastKind::Error:
		DrawCross(draw, centre, r, colour, stroke);
		return;
	case ToastKind::Warning:
		DrawBang(draw, centre, r, colour, stroke);
		return;
	default:
		DrawInfo(draw, centre, r, colour, stroke);
		return;
	}
}

struct Card
{
	ImVec2 start;
	ImVec2 end;
	float textLeft;
	float textTop;
	float wrap;
	float titleSize;
	float titleHeight;
	float bodySize;
};

Card Measure(const ToastQueue::Shown& toast, float right, float top)
{
	ImFont* const font = ImGui::GetFont();
	const float padding = Ui::Scaled(kPadding);
	const float radius = Ui::Scaled(kIconRadius);
	const float width = Ui::Scaled(kWidth);
	const float textOffset = Ui::Scaled(kStripe) + padding * 2.0f + radius * 2.0f;

	Card card = {};
	card.bodySize = ImGui::GetFontSize();
	card.titleSize = card.bodySize * kTitleScale;
	card.wrap = width - textOffset - padding;
	card.titleHeight = font->CalcTextSizeA(card.titleSize, FLT_MAX, 0.0f, toast.title).y;

	const float bodyHeight = font->CalcTextSizeA(card.bodySize, FLT_MAX, card.wrap, toast.text).y;
	const float textHeight = card.titleHeight + Ui::Scaled(kLineGap) + bodyHeight;
	const float height = (std::max)(textHeight, radius * 2.0f) + padding * 2.0f;
	const float slide = Ui::Scaled(kSlide) * (1.0f - toast.alpha);

	card.start = ImVec2(right - width + slide, top);
	card.end = ImVec2(card.start.x + width, top + height);
	card.textLeft = card.start.x + textOffset;
	card.textTop = top + (height - textHeight) * 0.5f;
	return card;
}

void DrawBackground(ImDrawList* draw, const Card& card, const ImVec4& accent, float alpha)
{
	const float rounding = Ui::Scaled(kRounding);
	const float drop = Ui::Scaled(kShadowDrop);
	const ImVec2 stripeEnd(card.start.x + Ui::Scaled(kStripe), card.end.y);

	draw->AddRectFilled(ImVec2(card.start.x, card.start.y + drop), ImVec2(card.end.x, card.end.y + drop),
		Faded(kShadow, alpha), rounding);
	draw->AddRectFilled(card.start, card.end, Faded(kCard, alpha), rounding);
	draw->AddRect(card.start, card.end, Faded(kEdge, alpha), rounding);
	draw->AddRectFilled(card.start, stripeEnd, Faded(accent, alpha), rounding, ImDrawFlags_RoundCornersLeft);
}

void DrawIcon(ImDrawList* draw, const Card& card, ToastKind kind, const ImVec4& accent, float alpha)
{
	const float radius = Ui::Scaled(kIconRadius);
	const ImVec2 centre(card.start.x + Ui::Scaled(kStripe) + Ui::Scaled(kPadding) + radius,
		(card.start.y + card.end.y) * 0.5f);

	draw->AddCircleFilled(centre, radius, Faded(accent, alpha));
	DrawGlyph(draw, centre, radius, kind, Faded(kCard, alpha));
}

void DrawTexts(ImDrawList* draw, const Card& card, const ToastQueue::Shown& toast, const ImVec4& accent)
{
	ImFont* const font = ImGui::GetFont();
	const float bodyTop = card.textTop + card.titleHeight + Ui::Scaled(kLineGap);

	draw->AddText(font, card.titleSize, ImVec2(card.textLeft, card.textTop), Faded(accent, toast.alpha),
		toast.title);
	draw->AddText(font, card.bodySize, ImVec2(card.textLeft, bodyTop), Faded(kText, toast.alpha), toast.text,
		nullptr, card.wrap);
}

void DrawTimeLeft(ImDrawList* draw, const Card& card, const ImVec4& accent, float alpha, float progress)
{
	const float left = card.start.x + Ui::Scaled(kStripe);
	const float right = left + (card.end.x - left) * (1.0f - progress);

	draw->AddRectFilled(ImVec2(left, card.end.y - Ui::Scaled(kBarHeight)), ImVec2(right, card.end.y),
		Faded(accent, alpha * kBarAlpha));
}

float DrawToast(ImDrawList* draw, const ToastQueue::Shown& toast, float right, float top)
{
	const Card card = Measure(toast, right, top);
	const ImVec4& accent = AccentOf(toast.kind);

	DrawBackground(draw, card, accent, toast.alpha);
	DrawIcon(draw, card, toast.kind, accent, toast.alpha);
	DrawTexts(draw, card, toast, accent);
	DrawTimeLeft(draw, card, accent, toast.alpha, toast.progress);

	return card.end.y - card.start.y;
}

}

void Toast::Show(ToastKind kind, const char* title, const char* format, ...)
{
	if (title == nullptr || format == nullptr)
		return;

	va_list args;
	va_start(args, format);
	char text[512] = {};
	const int written = vsnprintf(text, sizeof(text), Tr(format), args);
	va_end(args);

	if (written <= 0)
		return;

	g_queue.Push(kind, title, text);
}

bool Toast::IsShowing()
{
	return !g_queue.IsEmpty();
}

void Toast::Draw()
{
	g_queue.Update(ImGui::GetIO().DeltaTime);

	ToastQueue::Shown shown[ToastQueue::kMaxShown] = {};
	const int count = g_queue.Take(shown, ToastQueue::kMaxShown);

	if (count == 0)
		return;

	ImDrawList* const draw = ImGui::GetForegroundDrawList();
	const ImVec2 screen = ImGui::GetIO().DisplaySize;
	const float right = screen.x - Ui::Scaled(kMargin);
	float top = screen.y * kTopFraction;

	for (int index = 0; index < count; ++index)
		top += DrawToast(draw, shown[index], right, top) + Ui::Scaled(kGap);
}
