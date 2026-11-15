#pragma once

#include "Palette/EntrySelection.h"

#include <cstdint>

namespace EntryPicker
{
	constexpr int kNoEntry = -1;

	struct Colours
	{
		const uint8_t* rgba;
		const bool* changed;
	};

	struct GridResult
	{
		bool clicked;
		int hovered;
	};

	void DrawHint();

	GridResult DrawGrid(const unsigned char* entries, int count, const Colours& colours, EntrySelection& selection);

	void DrawEntryTooltip(int entry, bool changed);

	bool DrawFill(const Colours& colours, const EntrySelection& selection, bool& gradient, uint8_t (*outRgb)[3]);

	bool PickColour(const char* label, const uint8_t* rgb, uint8_t* outRgb);
}
