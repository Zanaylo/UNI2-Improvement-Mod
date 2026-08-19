// The effect pieces a coloredit pose is built from, out of data\_coloredit\edit_chrNNN.pat.
//
// A part is a textured 3D mesh: an atlas cut-out laid over a plane, ring, arc, sphere or cone,
// then scaled, rotated about all three axes and dropped through a perspective frustum. The atlas
// carries only shape and shading - the colour is one entry of the character's own palette, named
// per cut-out - which is what makes Vatista's wings follow the customiser.
// docs/CUSTOMIZE.md 7c has the whole derivation.

#pragma once

#include <cstdint>

namespace PatParts
{
	// Corners in the group's own space, wound the same way the cut-out's texture is.
	//
	// The shader is `col = texel * tint; col.rgb += add;` blended either additively or over, so a
	// quad carries everything that feeds it: the palette entry to tint by, a per-instance multiply
	// and an add. Nearly every effect is additive - that is what makes the game's bursts blow out
	// to white where they overlap.
	struct Quad
	{
		float x[4];
		float y[4];
		const uint8_t* pixels;
		int width;
		int height;
		// The slice of the cut-out this quad shows: a plane uses all of it, a ring or a sphere is
		// a mesh of them.
		float u0;
		float v0;
		float u1;
		float v1;
		int colour;
		bool additive;
		uint8_t multiply[4];
		uint8_t add[3];
	};

	bool Load(int chara);

	int GetQuadCount(int group);
	const Quad* GetQuads(int group);

	bool GetBounds(int group, float& outX0, float& outY0, float& outX1, float& outY1);
}
