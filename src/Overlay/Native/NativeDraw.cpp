#include "Overlay/Native/NativeDraw.h"

#include "D3D9/Draw/BitmapFont.h"
#include "D3D9/Draw/GameFont.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "D3D9/Draw/SystemText.h"
#include "Game/Menus/MenuWords.h"
#include "Overlay/Native/KeyGlyph.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr float kTitleTextPixels = 20.0f;
constexpr float kTitleIconGap = 1.0f;
constexpr float kTracking = 1.5f;

constexpr float kInfoLabelX = 82.0f;
constexpr float kInfoLabelDrop = 14.0f;
constexpr float kInfoRuleX = 189.0f;
constexpr float kInfoRuleDrop = 7.0f;
constexpr float kInfoRuleShort = 8.0f;
constexpr float kInfoTextX = 199.0f;
constexpr float kInfoTextDrop = 11.0f;
constexpr float kInfoTextRight = 1210.0f;
constexpr float kInfoPixels = 19.0f;
constexpr float kInfoLinePitch = 31.0f;
constexpr int kInfoLines = 3;

constexpr float kPromptTop = 683.0f;
constexpr float kPromptY = 702.0f;
constexpr float kPromptKeySize = 26.0f;
constexpr float kPromptPixels = 19.0f;

constexpr uint32_t kTitleInk = 0xFF141414;
constexpr uint32_t kText = 0xFFFFFFFF;
constexpr uint32_t kInfoPanel = 0xF0222326;
constexpr uint32_t kPromptBarColour = 0xB0141516;
constexpr uint32_t kPromptEdge = 0x40FFFFFF;

IDirect3DDevice9* g_device = nullptr;

bool TitleText(const NativeDraw::Frame& frame, const char* title, SystemText::Image& out)
{
	return SystemText::Render(g_device, title, static_cast<int>(frame.S(kTitleTextPixels) + 0.5f), out);
}

float FontScale(const NativeDraw::Frame& frame, const NativeDraw::Face& face, float pixels)
{
	const float line = face.font().GetLineHeight();

	return line > 0.0f ? frame.S(pixels) / line : 1.0f;
}

NativeDraw::Frame FrameFor(const D3DVIEWPORT9& viewport)
{
	NativeDraw::Frame frame = {};
	frame.scale = viewport.Height / NativeDraw::kReferenceHeight;
	frame.width = static_cast<float>(viewport.Width);
	frame.height = static_cast<float>(viewport.Height);
	frame.left = viewport.X + (frame.width - NativeDraw::kReferenceWidth * frame.scale) * 0.5f;
	frame.top = static_cast<float>(viewport.Y);

	return frame;
}

void WrappedText(const NativeDraw::Frame& frame, const char* text, float x, float top, float right)
{
	char line[NativeDraw::kLineBytes] = {};
	char candidate[NativeDraw::kLineBytes] = {};
	int lines = 0;

	const auto flush = [&]() {
		NativeDraw::Text(frame, line, x, top + kInfoPixels * 0.5f + lines * kInfoLinePitch, kInfoPixels, kText);
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

		if (used + length + 2 >= NativeDraw::kLineBytes)
			break;

		strcpy_s(candidate, line);
		if (used > 0)
			strcat_s(candidate, " ");
		strncat_s(candidate, word, length);

		if (used > 0 && NativeDraw::Width(frame, candidate, kInfoPixels) > right - x)
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

}

const NativeDraw::Face NativeDraw::kBodyFace = { &GameArt::Font, kTracking };

bool NativeDraw::Begin(IDirect3DDevice9* device, Frame& out)
{
	D3DVIEWPORT9 viewport = {};

	if (device == nullptr || FAILED(device->GetViewport(&viewport)) || viewport.Height == 0)
		return false;

	g_device = device;
	GameFont::Ensure(device);
	GameArt::Ensure(device);
	KeyGlyph::Observe();

	out = FrameFor(viewport);
	return true;
}

float NativeDraw::Width(const Frame& frame, const char* text, float pixels, const Face& face)
{
	return face.font().MeasureWidth(text, FontScale(frame, face, pixels), face.tracking) / frame.scale;
}

void NativeDraw::Text(const Frame& frame, const char* text, float x, float centreY, float pixels, uint32_t colour,
	const Face& face)
{
	face.font().Draw(text, frame.X(x), frame.Y(centreY - pixels * 0.5f), FontScale(frame, face, pixels), colour,
		face.tracking);
}

const char* NativeDraw::Fit(const Frame& frame, const char* text, float pixels, float width, char* out, size_t size)
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

void NativeDraw::Sprite(const Frame& frame, const GameArt::Sprite& sprite, float x, float y, float width, float height)
{
	GameArt::Draw(sprite, frame.X(x), frame.Y(y), frame.S(width), frame.S(height));
}

void NativeDraw::Wide(const Frame& frame, const GameArt::Sprite& sprite, float y, float height)
{
	GameArt::Draw(sprite, 0.0f, frame.Y(y), frame.width, frame.S(height));
}

void NativeDraw::Word(int index, const char* fallback, char* out)
{
	if (!MenuWords::Copy(index, out, kWordBytes))
		strcpy_s(out, kWordBytes, fallback);
}

float NativeDraw::TitleWidth(const Frame& frame, const char* title, const GameArt::Sprite& icon)
{
	SystemText::Image image = {};
	const float text = TitleText(frame, title, image) ? image.width / frame.scale : 0.0f;

	return static_cast<float>(icon.width) + kTitleIconGap + text;
}

void NativeDraw::Title(const Frame& frame, const char* title, float centreY, const GameArt::Sprite& icon)
{
	const float iconWidth = static_cast<float>(icon.width);
	const float iconHeight = static_cast<float>(icon.height);
	const float left = kCentre - TitleWidth(frame, title, icon) * 0.5f;

	Sprite(frame, icon, left, centreY - iconHeight * 0.5f, iconWidth, iconHeight);

	SystemText::Image image = {};

	if (!TitleText(frame, title, image))
		return;

	QuadRenderer::TexturedRect(image.texture, frame.X(left + iconWidth + kTitleIconGap),
		frame.Y(centreY) - image.height * 0.5f, image.width, image.height, 0.0f, 0.0f, image.u, image.v, kTitleInk);
}
void NativeDraw::Information(const Frame& frame, const char* text, const InfoBox& box)
{
	QuadRenderer::FillRect(0.0f, frame.Y(box.top), frame.width, frame.S(box.bottom - box.top), kInfoPanel);

	Sprite(frame, GameArt::kInformation, kInfoLabelX, box.top + kInfoLabelDrop,
		static_cast<float>(GameArt::kInformation.width), static_cast<float>(GameArt::kInformation.height));
	Sprite(frame, GameArt::kInformationRule, kInfoRuleX, box.top + kInfoRuleDrop,
		static_cast<float>(GameArt::kInformationRule.width), box.bottom - box.top - kInfoRuleDrop - kInfoRuleShort);

	WrappedText(frame, text, kInfoTextX, box.top + kInfoTextDrop, kInfoTextRight);
}

float NativeDraw::PromptKeys(const Frame& frame, const int* functions, int count, float right)
{
	float width = 0.0f;

	for (int i = count - 1; i >= 0; --i)
	{
		const float key = KeyGlyph::Width(functions[i], kPromptKeySize);

		KeyGlyph::Draw(functions[i], frame.X(right - width - key), frame.Y(kPromptY - kPromptKeySize * 0.5f),
			frame.S(kPromptKeySize));

		width += key;
	}

	return width;
}

float NativeDraw::PromptText(const Frame& frame, const char* text, float right)
{
	const float width = Width(frame, text, kPromptPixels);

	Text(frame, text, right - width, kPromptY, kPromptPixels, kText);
	return width;
}

void NativeDraw::PromptBar(const Frame& frame)
{
	QuadRenderer::FillRect(0.0f, frame.Y(kPromptTop), frame.width, frame.height - frame.Y(kPromptTop),
		kPromptBarColour);
	QuadRenderer::FillRect(0.0f, frame.Y(kPromptTop), frame.width, frame.S(1.0f), kPromptEdge);
}
