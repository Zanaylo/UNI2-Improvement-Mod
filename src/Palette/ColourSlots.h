#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ColourSlots
{
	enum class ExtendedOrder
	{
		OldestFirst = 0,
		NewestFirst = 1,
		ByName = 2,
		Custom = 3
	};

	constexpr int kGameColours = 0x2f;
	constexpr int kStockColours = 0x2a;
	constexpr int kCharacters = 64;
	constexpr int kNoColour = -1;
	constexpr int kNoExtended = 0;

	const char* BoundFile(int chara, int colour);
	void Label(int chara, int colour, char* out, size_t size);
	int ColourOf(int chara, const char* file);

	void Bind(int chara, int colour, const char* file);
	void Release(int chara, const char* file);

	int ExtendedCount(int chara);
	const char* ExtendedFile(int chara, int extended);
	int ExtendedOf(int chara, const char* file);

	ExtendedOrder GetExtendedOrder();
	void SetExtendedOrder(ExtendedOrder order);
	int CustomOrderRank(int chara, const char* file);
	void SetCustomOrder(int chara, const std::vector<std::string>& files);

	void Reload();
}
