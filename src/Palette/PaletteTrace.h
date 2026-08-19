#pragma once

namespace PaletteTrace
{
	void Reset();

	void Note(const char* format, ...);

	int GetCount();
	int GetDropped();

	int GetFrame(int index);
	const char* GetText(int index);
}
