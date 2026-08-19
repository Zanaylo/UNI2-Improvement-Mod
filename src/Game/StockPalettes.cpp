#include "Game/StockPalettes.h"

#include "Core/logger.h"
#include "Game/CharaTables.h"
#include "Game/ColorCustomize.h"
#include "Game/DataArchive.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace {

constexpr const char* kFolder = "_coloredit";

constexpr size_t kHeaderBytes = 16;
constexpr size_t kRowBytes = StockPalettes::kColours * 4;
constexpr size_t kCountOffset = 12;

struct Character
{
	std::vector<uint8_t> rows;
	int count = 0;
};

std::map<int, Character> g_characters;

int DistinctRows(const std::vector<uint8_t>& rows, int stored)
{
	const int limit = stored < ColorCustomize::kStockLimit ? stored : ColorCustomize::kStockLimit;

	int distinct = 1;

	for (int row = 1; row < limit; ++row)
	{
		if (memcmp(rows.data() + row * kRowBytes, rows.data(), kRowBytes) != 0)
			distinct = row + 1;
	}

	return distinct;
}

bool Parse(const std::vector<uint8_t>& data, Character& out)
{
	if (data.size() < kHeaderBytes + kRowBytes)
		return false;

	uint32_t stored = 0;
	memcpy(&stored, data.data() + kCountOffset, sizeof(stored));

	if (stored == 0 || data.size() < kHeaderBytes + stored * kRowBytes)
		return false;

	const size_t keep = stored < ColorCustomize::kStockLimit ? stored : ColorCustomize::kStockLimit;

	out.rows.assign(data.begin() + kHeaderBytes,
		data.begin() + kHeaderBytes + keep * kRowBytes);

	out.count = DistinctRows(out.rows, static_cast<int>(keep));
	return true;
}

const Character* Find(int chara)
{
	const auto found = g_characters.find(chara);
	if (found == g_characters.end() || found->second.count == 0)
		return nullptr;

	return &found->second;
}

}

bool StockPalettes::Load(int chara)
{
	if (chara < 0 || chara >= CharaTables::GetCharaCount())
		return false;

	if (g_characters.find(chara) != g_characters.end())
		return g_characters[chara].count > 0;

	Character& character = g_characters[chara];

	char file[32] = {};
	sprintf_s(file, "chr%03d.pal", chara);

	std::vector<uint8_t> data;

	if (!DataArchive::Read(kFolder, file, data) || !Parse(data, character))
	{
		LOG("coloredit: %s could not be read", file);
		return false;
	}

	return true;
}

int StockPalettes::GetCount(int chara)
{
	const Character* const character = Find(chara);
	if (character == nullptr)
		return 0;

	return character->count;
}

const uint8_t* StockPalettes::GetRow(int chara, int palette)
{
	const Character* const character = Find(chara);
	if (character == nullptr || palette < 0 || palette >= character->count)
		return nullptr;

	return character->rows.data() + static_cast<size_t>(palette) * kRowBytes;
}
