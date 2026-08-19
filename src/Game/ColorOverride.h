#pragma once

#include <cstdint>

namespace ColorOverride
{
	bool Has(int chara, int slot, int part);
	bool Get(int chara, int slot, int part, uint8_t* outRgb);

	void Set(int chara, int slot, int part, const uint8_t* rgb);
	void Clear(int chara, int slot, int part);
	void ClearSlot(int chara, int slot);

	bool AnyInSlot(int chara, int slot);

	void Retint(uint8_t* palette, const int* entries, int count, const uint8_t* reference,
		const uint8_t* rgb);
}
