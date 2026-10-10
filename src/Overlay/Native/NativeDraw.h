#pragma once

#include "Overlay/Native/GameArt.h"

#include <d3d9.h>

#include <cstddef>
#include <cstdint>

class BitmapFont;

namespace NativeDraw
{
	constexpr float kReferenceWidth = 1280.0f;
	constexpr float kReferenceHeight = 720.0f;
	constexpr float kCentre = kReferenceWidth * 0.5f;
	constexpr size_t kLineBytes = 256;
	constexpr size_t kWordBytes = 96;

	struct Frame
	{
		float scale;
		float left;
		float top;
		float width;
		float height;

		float X(float x) const { return left + x * scale; }
		float Y(float y) const { return top + y * scale; }
		float S(float value) const { return value * scale; }
	};

	struct Face
	{
		BitmapFont& (*font)();
		float tracking;
	};

	extern const Face kBodyFace;

	struct InfoBox
	{
		float top;
		float bottom;
	};

	bool Begin(IDirect3DDevice9* device, Frame& out);

	float Width(const Frame& frame, const char* text, float pixels, const Face& face = kBodyFace);
	void Text(const Frame& frame, const char* text, float x, float centreY, float pixels, uint32_t colour,
		const Face& face = kBodyFace);
	const char* Fit(const Frame& frame, const char* text, float pixels, float width, char* out, size_t size);

	void Sprite(const Frame& frame, const GameArt::Sprite& sprite, float x, float y, float width, float height);
	void Wide(const Frame& frame, const GameArt::Sprite& sprite, float y, float height);

	void Word(int index, const char* fallback, char* out);

	float TitleWidth(const Frame& frame, const char* title, const GameArt::Sprite& icon);
	void Title(const Frame& frame, const char* title, float centreY, const GameArt::Sprite& icon);
	void Information(const Frame& frame, const char* text, const InfoBox& box);

	float PromptKeys(const Frame& frame, const int* functions, int count, float right);
	float PromptText(const Frame& frame, const char* text, float right);
	void PromptBar(const Frame& frame);
}
