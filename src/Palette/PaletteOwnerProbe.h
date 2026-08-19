#pragma once

#include <cstdint>

namespace PaletteOwnerProbe
{
	struct Row
	{
		uintptr_t owner;
		uintptr_t texture;
		uintptr_t override;
		int row;
		int charaFromStack;
		int stackDepth;
		int draws;
	};

	bool Install();

	void SetEnabled(bool enabled);
	bool IsEnabled();

	int GetCount();
	bool Get(int index, Row& out);

	void Reset();
}
