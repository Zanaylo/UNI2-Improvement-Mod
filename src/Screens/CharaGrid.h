#pragma once

#include "Screens/PatFile.h"
#include "Screens/PatPainter.h"

#include <cstdint>
#include <string>
#include <vector>

#include <d3d9.h>

namespace CharaGrid
{
	constexpr int kSideCount = 2;
	constexpr int kNoSlot = -1;

	struct Style
	{
		PatFile::Handle pat;
		const std::vector<std::string>* codes;
		std::string cursor;
		std::string tag[kSideCount];
		float tagX[kSideCount];
		float tagY[kSideCount];
		float zoom;
		float spreadX;
		float spreadY;
		int fps;
		uint32_t tint[kSideCount][2];
	};

	bool Prepare(IDirect3DDevice9* device);
	void SetStyle(const Style& style);

	int Draw(const PatPainter::Placement& where, unsigned elapsedMs);

	void Invalidate();

	int SlotCount();

	void SetCursor(int side, int slot);
	void SetCursorFromId(int side, uint32_t id);
	int GetCursor(int side);

	const char* StatusText();
}
