// The character, drawn in the overlay with the colours being edited.
//
// The pose comes back from CgImage as one byte per pixel, so a repaint is a 256-entry lookup over
// the frame - cheap enough to do the moment a colour changes, and never on a frame that has not.

#pragma once

#include "Game/PatParts.h"

#include <cstdint>

struct IDirect3DTexture9;

class CharaPreview
{
public:
	~CharaPreview();

	void Release();

	int GetFrameCount(int chara);

	bool Draw(int chara, int frame, const uint8_t* palette, float maxWidth, float maxHeight);

private:
	bool Ensure(int chara, int frame, const uint8_t* palette);
	bool Create(int width, int height);
	bool Paint(const uint8_t* indices, const uint8_t* palette);

	// One shaded corner of a face: where it lands and which texel it shows.
	struct Vertex
	{
		float x;
		float y;
		float u;
		float v;
	};

	void DrawParts(bool behind, const uint8_t* palette, uint8_t* pixels, int pitch) const;
	void DrawQuad(const PatParts::Quad& quad, float offsetX, float offsetY, const uint8_t* palette,
		uint8_t* pixels, int pitch) const;
	void DrawTriangle(const PatParts::Quad& quad, const Vertex& a, const Vertex& b, const Vertex& c,
		const uint8_t* tint, uint8_t* pixels, int pitch) const;

	IDirect3DTexture9* m_texture = nullptr;
	int m_chara = -1;
	int m_frame = -1;
	int m_width = 0;
	int m_height = 0;
	uint8_t m_palette[256 * 4] = {};
	bool m_painted = false;
};
