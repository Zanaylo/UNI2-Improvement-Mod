#include "Game/StageThumb.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/DataArchive.h"
#include "Game/MbtlCipher.h"
#include "Game/StageArchive.h"
#include "Game/StageImport.h"

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

constexpr int kDfciColumns = 9;
constexpr int kDfciCellWidth = 209;
constexpr int kDfciCellHeight = 145;
constexpr int kDfciFirstX = 8;
constexpr int kDfciFirstY = 0;
constexpr int kDfciPitchX = 224;
constexpr int kDfciPitchY = 256;

constexpr const char* kDfciSheet = "grpdat\\CSel\\stage_thum00.dds";

constexpr const char* kUniSheet = "grpdat\\CSel\\stage_sam00.dds";
constexpr int kUniRows = 16;
constexpr int kUniCells = 21;
constexpr int kUniCellWidth = 956;
constexpr int kUniCellHeight = 128;
constexpr int kUniFirstX = 43;
constexpr int kUniFirstY = 0;
constexpr int kUniPitchX = 1006;
constexpr int kUniPitchY = 128;

constexpr const char* kOurFolder = "CSel";
constexpr const char* kCardFolder = "Mods\\CSel";
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

void Rgb565(uint16_t packed, uint8_t* out)
{
	out[2] = static_cast<uint8_t>((((packed >> 11) & 0x1f) * 255) / 31);
	out[1] = static_cast<uint8_t>((((packed >> 5) & 0x3f) * 255) / 63);
	out[0] = static_cast<uint8_t>(((packed & 0x1f) * 255) / 31);
}

bool DecodeDxt(const std::vector<uint8_t>& blob, Image& out)
{
	if (blob.size() < kDdsHeader || memcmp(blob.data(), "DDS ", 4) != 0)
		return false;

	const uint32_t flags = ReadLittle32(blob, 80);
	const uint32_t fourcc = ReadLittle32(blob, 84);

	if ((flags & 4) == 0)
		return Unpack(blob, out);

	const bool five = fourcc == 0x35545844;
	const bool one = fourcc == 0x31545844;

	if (!five && !one)
		return false;

	out.height = static_cast<int>(ReadLittle32(blob, 12));
	out.width = static_cast<int>(ReadLittle32(blob, 16));

	if (out.width <= 0 || out.height <= 0 || out.width > 8192 || out.height > 8192)
		return false;

	const int blocksX = (out.width + 3) / 4;
	const int blocksY = (out.height + 3) / 4;
	const size_t stride = five ? 16 : 8;

	if (blob.size() < kDdsHeader + static_cast<size_t>(blocksX) * blocksY * stride)
		return false;

	out.pixels.assign(static_cast<size_t>(out.width) * out.height * 4, 0);

	for (int by = 0; by < blocksY; ++by)
	{
		for (int bx = 0; bx < blocksX; ++bx)
		{
			const uint8_t* const block = blob.data() + kDdsHeader
				+ (static_cast<size_t>(by) * blocksX + bx) * stride;
			const uint8_t* const colour = five ? block + 8 : block;

			uint8_t alpha[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };
			uint64_t alphaBits = 0;

			if (five)
			{
				alpha[0] = block[0];
				alpha[1] = block[1];

				if (alpha[0] > alpha[1])
				{
					for (int i = 1; i < 7; ++i)
						alpha[i + 1] = static_cast<uint8_t>(((7 - i) * alpha[0] + i * alpha[1]) / 7);
				}
				else
				{
					for (int i = 1; i < 5; ++i)
						alpha[i + 1] = static_cast<uint8_t>(((5 - i) * alpha[0] + i * alpha[1]) / 5);

					alpha[6] = 0;
					alpha[7] = 255;
				}

				for (int i = 0; i < 6; ++i)
					alphaBits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
			}

			const uint16_t c0 = static_cast<uint16_t>(colour[0] | (colour[1] << 8));
			const uint16_t c1 = static_cast<uint16_t>(colour[2] | (colour[3] << 8));

			uint8_t palette[4][4] = {};
			Rgb565(c0, palette[0]);
			Rgb565(c1, palette[1]);

			for (int i = 0; i < 3; ++i)
			{
				if (c0 > c1 || five)
				{
					palette[2][i] = static_cast<uint8_t>((2 * palette[0][i] + palette[1][i]) / 3);
					palette[3][i] = static_cast<uint8_t>((palette[0][i] + 2 * palette[1][i]) / 3);
				}
				else
				{
					palette[2][i] = static_cast<uint8_t>((palette[0][i] + palette[1][i]) / 2);
					palette[3][i] = 0;
				}
			}

			const uint32_t bits = ReadLittle32(blob, static_cast<size_t>(colour - blob.data()) + 4);

			for (int py = 0; py < 4; ++py)
			{
				for (int px = 0; px < 4; ++px)
				{
					const int x = bx * 4 + px;
					const int y = by * 4 + py;

					if (x >= out.width || y >= out.height)
						continue;

					const int i = py * 4 + px;
					const uint8_t* const src = palette[(bits >> (2 * i)) & 3];
					uint8_t* const dst = &out.pixels[(static_cast<size_t>(y) * out.width + x) * 4];

					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = five ? alpha[(alphaBits >> (3 * i)) & 7]
						: static_cast<uint8_t>((c0 > c1 || ((bits >> (2 * i)) & 3) != 3) ? 255 : 0);
				}
			}
		}
	}

	return true;
}

bool DfciSheet(const std::string& gameFolder, std::vector<uint8_t>& blob)
{
	std::string path = gameFolder;

	if (!path.empty() && path.back() != '\\' && path.back() != '/')
		path.push_back('\\');

	return ReadWholeFile(path + kDfciSheet, blob) && !blob.empty();
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

constexpr int kBackdropShade = 42;
constexpr int kBlurRadius = 5;

void BoxBlur(std::vector<uint8_t>& pixels, int width, int height)
{
	std::vector<uint8_t> pass(pixels.size());

	for (int channel = 0; channel < 3; ++channel)
	{
		for (int row = 0; row < height; ++row)
		{
			for (int column = 0; column < width; ++column)
			{
				int total = 0;
				int count = 0;

				for (int step = -kBlurRadius; step <= kBlurRadius; ++step)
				{
					const int at = column + step;

					if (at < 0 || at >= width)
						continue;

					total += pixels[(static_cast<size_t>(row) * width + at) * 4 + channel];
					++count;
				}

				pass[(static_cast<size_t>(row) * width + column) * 4 + channel] =
					static_cast<uint8_t>(total / count);
			}
		}

		for (int column = 0; column < width; ++column)
		{
			for (int row = 0; row < height; ++row)
			{
				int total = 0;
				int count = 0;

				for (int step = -kBlurRadius; step <= kBlurRadius; ++step)
				{
					const int at = row + step;

					if (at < 0 || at >= height)
						continue;

					total += pass[(static_cast<size_t>(at) * width + column) * 4 + channel];
					++count;
				}

				pixels[(static_cast<size_t>(row) * width + column) * 4 + channel] =
					static_cast<uint8_t>(total / count);
			}
		}
	}
}

void BlitCard(const Image& source, int sourceX, int sourceY, int sourceWidth, int sourceHeight,
	Image& target, int targetX, int targetY)
{
	std::vector<uint8_t> cell(static_cast<size_t>(kOurCellWidth) * kOurCellHeight * 4);

	int coverWidth = (sourceHeight * kOurCellWidth) / kOurCellHeight;

	if (coverWidth > sourceWidth || coverWidth < 1)
		coverWidth = sourceWidth;

	const int coverX = sourceX + (sourceWidth - coverWidth) / 2;

	for (int row = 0; row < kOurCellHeight; ++row)
	{
		const int from = sourceY + (row * sourceHeight) / kOurCellHeight;

		for (int column = 0; column < kOurCellWidth; ++column)
		{
			const int at = coverX + (column * coverWidth) / kOurCellWidth;
			const uint8_t* const in = &source.pixels[(static_cast<size_t>(from) * source.width +
				at) * 4];
			uint8_t* const out = &cell[(static_cast<size_t>(row) * kOurCellWidth + column) * 4];

			for (int channel = 0; channel < 3; ++channel)
				out[channel] = static_cast<uint8_t>((in[channel] * kBackdropShade) / 100);

			out[3] = 0xff;
		}
	}

	BoxBlur(cell, kOurCellWidth, kOurCellHeight);

	const int fitted = (kOurCellWidth * sourceHeight) / sourceWidth;
	const int top = (kOurCellHeight - fitted) / 2;

	for (int row = 0; row < fitted; ++row)
	{
		const int from = sourceY + (row * sourceHeight) / fitted;

		for (int column = 0; column < kOurCellWidth; ++column)
		{
			const int at = sourceX + (column * sourceWidth) / kOurCellWidth;
			const uint8_t* const in = &source.pixels[(static_cast<size_t>(from) * source.width +
				at) * 4];
			uint8_t* const out = &cell[(static_cast<size_t>(top + row) * kOurCellWidth +
				column) * 4];

			for (int channel = 0; channel < 3; ++channel)
				out[channel] = in[channel];

			out[3] = 0xff;
		}
	}

	for (int row = 0; row < kOurCellHeight; ++row)
	{
		memcpy(&target.pixels[(static_cast<size_t>(targetY + row) * target.width + targetX) * 4],
			&cell[static_cast<size_t>(row) * kOurCellWidth * 4],
			static_cast<size_t>(kOurCellWidth) * 4);
	}
}

void KeepCard(const Image& sheet, int x, int y, int number)
{
	std::vector<uint8_t> cell(static_cast<size_t>(kOurCellWidth) * kOurCellHeight * 4);

	for (int row = 0; row < kOurCellHeight; ++row)
	{
		memcpy(&cell[static_cast<size_t>(row) * kOurCellWidth * 4],
			&sheet.pixels[(static_cast<size_t>(y + row) * sheet.width + x) * 4],
			static_cast<size_t>(kOurCellWidth) * 4);
	}

	CreateDirectoryTree(GetModRootPath(kCardFolder));

	FILE* handle = nullptr;

	if (fopen_s(&handle, StageThumb::CardPath(number).c_str(), "wb") != 0 || handle == nullptr)
		return;

	fwrite(cell.data(), 1, cell.size(), handle);
	fclose(handle);
}

bool Paint(const Image& card, int cardX, int cardY, int cardWidth, int cardHeight, int number,
	bool fit = false)
{
	const int cell = StageThumb::CellFor(number) - kOurSecondSheet;

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

	if (fit)
		BlitCard(card, cardX, cardY, cardWidth, cardHeight, ours, x, y);
	else
		Blit(card, cardX, cardY, cardWidth, cardHeight, ours, x, y);

	memcpy(blob.data() + kDdsHeader, ours.pixels.data(), ours.pixels.size());
	KeepCard(ours, x, y, number);

	return Write(1, blob);
}

bool TakeDfci(const std::string& gameFolder, int sourceCell, int number)
{
	std::vector<uint8_t> raw;

	if (!DfciSheet(gameFolder, raw))
	{
		LOG("StageThumb: that DFCI build has no picker sheet at %s", kDfciSheet);
		return false;
	}

	Image sheet;

	if (!DecodeDxt(raw, sheet))
	{
		LOG("StageThumb: that DFCI picker sheet is not a DDS the mod can decode");
		return false;
	}

	int cardX = 0;
	int cardY = 0;
	CellOf(sourceCell, kDfciColumns, kDfciFirstX, kDfciFirstY, kDfciPitchX, kDfciPitchY,
		cardX, cardY);

	if (cardX + kDfciCellWidth > sheet.width || cardY + kDfciCellHeight > sheet.height)
		return false;

	if (!Paint(sheet, cardX, cardY, kDfciCellWidth, kDfciCellHeight, number, true))
		return false;

	LOG("StageThumb: stage %d takes cell %d of DFCI's picker", number, sourceCell);
	return true;
}

bool TakeUni(const std::string& gameFolder, int sourceCell, int number)
{
	const int index = sourceCell - 1;

	if (index < 0 || index >= kUniCells)
		return false;

	std::vector<uint8_t> raw;

	if (!StageArchive::Asset(gameFolder.c_str(), kUniSheet, raw))
	{
		LOG("StageThumb: that UNI build has no picker sheet at %s", kUniSheet);
		return false;
	}

	Image sheet;

	if (!DecodeDxt(raw, sheet))
	{
		LOG("StageThumb: that UNI picker sheet is not a DDS the mod can decode");
		return false;
	}

	const int cardX = kUniFirstX + (index / kUniRows) * kUniPitchX;
	const int cardY = kUniFirstY + (index % kUniRows) * kUniPitchY;

	if (cardX + kUniCellWidth > sheet.width || cardY + kUniCellHeight > sheet.height)
		return false;

	const int window = (kUniCellHeight * kOurCellWidth) / kOurCellHeight;
	const int from = cardX + (kUniCellWidth - window) / 2;

	if (!Paint(sheet, from, cardY, window, kUniCellHeight, number))
		return false;

	LOG("StageThumb: stage %d takes cell %d of UNI's stage select", number, sourceCell);
	return true;
}

bool TakeMbtl(const std::string& gameFolder, int sourceCell, int number)
{
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

}

int StageThumb::CellFor(int number)
{
	if (number < StageImport::kFirstNumber || number > StageImport::kLastNumber)
		return -1;

	return kFirstCell + (number - kFirstCell) % kCells;
}

std::string StageThumb::CardPath(int number)
{
	char leaf[32] = {};
	sprintf_s(leaf, "\\card%03d.bin", number);

	return GetModRootPath(kCardFolder) + leaf;
}

bool StageThumb::HasCard(int number)
{
	return GetFileAttributesA(CardPath(number).c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool StageThumb::Take(FbGameFolder::Game game, const std::string& gameFolder, int sourceCell,
	int number)
{
	if (sourceCell < 0 || CellFor(number) < 0)
		return false;

	if (game == FbGameFolder::Game_DFCI)
		return TakeDfci(gameFolder, sourceCell, number);

	if (game == FbGameFolder::Game_UNI)
		return TakeUni(gameFolder, sourceCell, number);

	if (game == FbGameFolder::Game_MBTL)
		return TakeMbtl(gameFolder, sourceCell, number);

	return false;
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
