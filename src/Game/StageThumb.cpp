#include "Game/StageThumb.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/DataArchive.h"
#include "Game/MbtlCipher.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr size_t kDdsHeader = 128;
constexpr int kSheetSide = 1024;

constexpr int kOurColumns = 8;
constexpr int kOurCellWidth = 120;
constexpr int kOurCellHeight = 320;
constexpr int kOurFirstX = 4;
constexpr int kOurFirstY = 2;
constexpr int kOurPitchX = 128;
constexpr int kOurPitchY = 336;
constexpr int kOurSecondSheet = 24;

constexpr int kMbtlColumns = 7;
constexpr int kMbtlCellWidth = 104;
constexpr int kMbtlCellHeight = 330;
constexpr int kMbtlFirstX = 20;
constexpr int kMbtlFirstY = 3;
constexpr int kMbtlPitchX = 144;
constexpr int kMbtlPitchY = 336;
constexpr int kMbtlSecondSheet = 21;

constexpr int kMbtlCardX = 4;
constexpr int kMbtlCardY = 50;
constexpr int kMbtlCardWidth = 96;
constexpr int kMbtlCardHeight = 230;

constexpr const char* kOurFolder = "CSel";
constexpr const char* kOurSheets[] = { "stage_thumb00.dds", "stage_thumb01.dds" };

constexpr const char* kMbtlArchive = "data010.bin";

constexpr uint32_t kMbtlThumb00Offset = 452827714;
constexpr uint32_t kMbtlThumb01Offset = 457022146;
constexpr uint32_t kDriftSpan = 0x100000;
constexpr uint32_t kPhaseCount = 0x400;

struct Image
{
	std::vector<uint8_t> pixels;
	int width;
	int height;
};

std::string OurPath(int sheet)
{
	char leaf[64] = {};
	sprintf_s(leaf, "Mods\\grpdat\\CSel\\%s", kOurSheets[sheet]);

	return GetModRootPath(leaf);
}

bool Unpack(const std::vector<uint8_t>& blob, Image& out)
{
	if (blob.size() < kDdsHeader || memcmp(blob.data(), "DDS ", 4) != 0)
		return false;

	out.height = static_cast<int>(ReadLittle32(blob, 12));
	out.width = static_cast<int>(ReadLittle32(blob, 16));

	const size_t need = static_cast<size_t>(out.width) * out.height * 4;

	if (out.width <= 0 || out.height <= 0 || blob.size() < kDdsHeader + need)
		return false;

	out.pixels.assign(blob.begin() + kDdsHeader, blob.begin() + kDdsHeader + need);
	return true;
}

bool OurSheet(int sheet, std::vector<uint8_t>& blob)
{
	if (ReadWholeFile(OurPath(sheet), blob) && !blob.empty())
		return true;

	return DataArchive::Read(kOurFolder, kOurSheets[sheet], blob) && !blob.empty();
}

bool Write(int sheet, const std::vector<uint8_t>& blob)
{
	const std::string path = OurPath(sheet);
	const size_t leaf = path.rfind('\\');

	if (leaf == std::string::npos)
		return false;

	CreateDirectoryTree(path.substr(0, leaf));

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const size_t written = fwrite(blob.data(), 1, blob.size(), handle);
	fclose(handle);

	return written == blob.size();
}

bool ReadAt(FILE* handle, uint32_t offset, size_t size, std::vector<uint8_t>& out)
{
	if (fseek(handle, static_cast<long>(offset), SEEK_SET) != 0)
		return false;

	out.resize(size);
	const size_t read = fread(out.data(), 1, out.size(), handle);
	out.resize(read);

	return read > 0;
}

bool HeaderAt(const std::vector<uint8_t>& window, size_t at, uint32_t& outPhase)
{
	if (at + 32 > window.size() || window[at] != 0xe1 || window[at + 1] != 0x5c)
		return false;

	for (uint32_t phase = 0; phase < kPhaseCount; ++phase)
	{
		std::vector<uint8_t> head(window.begin() + at, window.begin() + at + 32);
		MbtlCipher::DecryptAt(head, phase);

		if (memcmp(head.data(), "DDS ", 4) != 0 || ReadLittle32(head, 4) != 124)
			continue;

		const int height = static_cast<int>(ReadLittle32(head, 12));
		const int width = static_cast<int>(ReadLittle32(head, 16));

		if (width != kSheetSide || height != kSheetSide)
			return false;

		outPhase = phase;
		return true;
	}

	return false;
}

bool MbtlSheet(const std::string& gameFolder, uint32_t recorded, std::vector<uint8_t>& out)
{
	std::string path = gameFolder;

	if (!path.empty() && path.back() != '\\' && path.back() != '/')
		path.push_back('\\');

	path += kMbtlArchive;

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	std::vector<uint8_t> window;
	const bool read = ReadAt(handle, recorded, kDriftSpan, window);

	uint32_t phase = 0;
	size_t at = 0;
	bool found = false;

	for (; read && at + 32 < window.size(); ++at)
	{
		if (HeaderAt(window, at, phase))
		{
			found = true;
			break;
		}
	}

	const size_t size = kDdsHeader + static_cast<size_t>(kSheetSide) * kSheetSide * 4;
	std::vector<uint8_t> raw;
	const bool whole = found && ReadAt(handle, recorded + static_cast<uint32_t>(at), size, raw) &&
		raw.size() == size;

	fclose(handle);

	if (!whole)
		return false;

	MbtlCipher::DecryptAt(raw, phase);
	out.swap(raw);

	return memcmp(out.data(), "DDS ", 4) == 0;
}

void CellOf(int index, int columns, int firstX, int firstY, int pitchX, int pitchY, int& outX,
	int& outY)
{
	outX = firstX + (index % columns) * pitchX;
	outY = firstY + (index / columns) * pitchY;
}

void Blit(const Image& source, int sourceX, int sourceY, int sourceWidth, int sourceHeight,
	Image& target, int targetX, int targetY)
{
	for (int row = 0; row < kOurCellHeight; ++row)
	{
		const int from = sourceY + (row * sourceHeight) / kOurCellHeight;
		uint8_t* const out = &target.pixels[(static_cast<size_t>(targetY + row) * target.width +
			targetX) * 4];

		for (int column = 0; column < kOurCellWidth; ++column)
		{
			const int at = sourceX + (column * sourceWidth) / kOurCellWidth;
			const uint8_t* const in = &source.pixels[(static_cast<size_t>(from) * source.width +
				at) * 4];

			out[column * 4 + 0] = in[0];
			out[column * 4 + 1] = in[1];
			out[column * 4 + 2] = in[2];
			out[column * 4 + 3] = 0xff;
		}
	}
}

bool Paint(const Image& card, int cardX, int cardY, int cardWidth, int cardHeight, int number)
{
	const int cell = number - kOurSecondSheet;

	std::vector<uint8_t> blob;

	if (!OurSheet(1, blob))
		return false;

	Image ours;

	if (!Unpack(blob, ours))
		return false;

	int x = 0;
	int y = 0;
	CellOf(cell, kOurColumns, kOurFirstX, kOurFirstY, kOurPitchX, kOurPitchY, x, y);

	if (x + kOurCellWidth > ours.width || y + kOurCellHeight > ours.height)
		return false;

	Blit(card, cardX, cardY, cardWidth, cardHeight, ours, x, y);

	memcpy(blob.data() + kDdsHeader, ours.pixels.data(), ours.pixels.size());

	return Write(1, blob);
}

}

bool StageThumb::Take(FbGameFolder::Game game, const std::string& gameFolder, int sourceCell,
	int number)
{
	if (game != FbGameFolder::Game_MBTL || sourceCell < 0 || number < kFirstCell ||
		number > kLastCell)
	{
		return false;
	}

	const bool second = sourceCell >= kMbtlSecondSheet;
	std::vector<uint8_t> blob;

	if (!MbtlSheet(gameFolder, second ? kMbtlThumb01Offset : kMbtlThumb00Offset, blob))
	{
		LOG("StageThumb: that MELTY BLOOD build's picker sheet could not be located");
		return false;
	}

	Image sheet;

	if (!Unpack(blob, sheet))
		return false;

	int x = 0;
	int y = 0;
	CellOf(second ? sourceCell - kMbtlSecondSheet : sourceCell, kMbtlColumns, kMbtlFirstX,
		kMbtlFirstY, kMbtlPitchX, kMbtlPitchY, x, y);

	if (x + kMbtlCellWidth > sheet.width || y + kMbtlCellHeight > sheet.height)
		return false;

	if (!Paint(sheet, x + kMbtlCardX, y + kMbtlCardY, kMbtlCardWidth, kMbtlCardHeight, number))
		return false;

	LOG("StageThumb: stage %d takes cell %d of MELTY BLOOD's picker", number, sourceCell);
	return true;
}

bool StageThumb::Drop(int number)
{
	if (number < kFirstCell || number > kLastCell)
		return false;

	std::vector<uint8_t> ours;
	std::vector<uint8_t> vanilla;

	if (!ReadWholeFile(OurPath(1), ours) ||
		!DataArchive::Read(kOurFolder, kOurSheets[1], vanilla) || vanilla.size() != ours.size())
	{
		return false;
	}

	Image blank;
	Image target;

	if (!Unpack(vanilla, blank) || !Unpack(ours, target))
		return false;

	int x = 0;
	int y = 0;
	CellOf(number - kOurSecondSheet, kOurColumns, kOurFirstX, kOurFirstY, kOurPitchX, kOurPitchY,
		x, y);

	for (int row = 0; row < kOurCellHeight; ++row)
	{
		const size_t at = (static_cast<size_t>(y + row) * target.width + x) * 4;
		memcpy(&target.pixels[at], &blank.pixels[at], static_cast<size_t>(kOurCellWidth) * 4);
	}

	memcpy(ours.data() + kDdsHeader, target.pixels.data(), target.pixels.size());

	return Write(1, ours);
}
