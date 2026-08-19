#pragma once

#include <cstdint>

namespace PaletteIdentity
{

	enum class Source : uint8_t
	{
		None,
		Guess,
		Elimination,
		Colours,
		CharaStack,
		Hand,
	};

	void OnFrame();

	void Reset();

	Source GetSource(int textureIndex);
	int GetChara(int textureIndex);
	const char* GetSourceName(Source source);

	bool ColoursEverMatched();
	int GetComparisons();

	int GetCatalogueCount(int player);
}
