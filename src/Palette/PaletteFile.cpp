#include "Palette/PaletteFile.h"

#include "Core/logger.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t kPalHeader[4] = { 0x0000ffff, 1, 0, 1 };
constexpr int kPalHeaderBytes = 16;

constexpr char kTrailerMagic[8] = { 'U', 'N', 'I', '2', 'I', 'M', 'P', 'L' };

struct Trailer
{
	char magic[8];
	PaletteFile::Info info;
};

constexpr char kLegacyMagic[4] = { 'U', 'I', 'P', 'L' };

struct LegacyHeader
{
	char magic[4];
	int32_t version;
	PaletteFile::Info info;
};

void NameFromPath(const std::string& path, char* out, int size)
{
	const size_t slash = path.find_last_of("\\/");
	const size_t dot = path.find_last_of('.');

	const size_t start = slash == std::string::npos ? 0 : slash + 1;
	const size_t end = dot == std::string::npos || dot < start ? path.size() : dot;

	const std::string name = path.substr(start, end - start);
	strncpy_s(out, size, name.c_str(), _TRUNCATE);
}

long PaletteStart(const uint8_t* head, long size)
{
	uint32_t count = 0;
	memcpy(&count, head, sizeof(count));

	if (count > 0 && static_cast<long>(count) * PaletteFile::kBytes + 4 <= size)
		return 4;

	memcpy(&count, head + 12, sizeof(count));

	if (count > 0 && static_cast<long>(count) * PaletteFile::kBytes + kPalHeaderBytes <= size)
		return kPalHeaderBytes;

	return 0;
}

bool ReadTrailer(FILE* file, long from, long size, PaletteFile::Info& info)
{
	if (size - from < static_cast<long>(sizeof(Trailer)))
		return false;

	Trailer trailer = {};
	if (fseek(file, from, SEEK_SET) != 0 || fread(&trailer, 1, sizeof(trailer), file) !=
		sizeof(trailer))
	{
		return false;
	}

	if (memcmp(trailer.magic, kTrailerMagic, sizeof(kTrailerMagic)) != 0)
		return false;

	info = trailer.info;
	return true;
}

}

bool PaletteFile::Load(const std::string& path, uint8_t* colors, Info& info, uint8_t* effectColors,
	bool* outHasEffect)
{
	if (outHasEffect != nullptr)
		*outHasEffect = false;

	if (colors == nullptr)
		return false;

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
		return false;

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	memset(&info, 0, sizeof(info));
	bool ok = false;

	if (size == kBytes)
	{

		ok = fread(colors, 1, kBytes, file) == kBytes;
	}
	else if (size == static_cast<long>(sizeof(LegacyHeader) + kBytes))
	{
		LegacyHeader header = {};
		if (fread(&header, 1, sizeof(header), file) == sizeof(header) &&
			memcmp(header.magic, kLegacyMagic, sizeof(kLegacyMagic)) == 0)
		{
			info = header.info;
			ok = fread(colors, 1, kBytes, file) == kBytes;
		}
	}
	else if (size > kPalHeaderBytes)
	{
		uint8_t head[kPalHeaderBytes] = {};

		if (fread(head, 1, sizeof(head), file) == sizeof(head))
		{
			const long start = PaletteStart(head, size);

			if (start > 0 && fseek(file, start, SEEK_SET) == 0)
			{
				ok = fread(colors, 1, kBytes, file) == kBytes;

				uint32_t count = 0;
				memcpy(&count, head + (start == 4 ? 0 : 12), sizeof(count));

				if (ok && count >= 2 && effectColors != nullptr &&
					fread(effectColors, 1, kBytes, file) == kBytes && outHasEffect != nullptr)
				{
					*outHasEffect = true;
				}

				ReadTrailer(file, start + static_cast<long>(count) * kBytes, size, info);
			}
		}
	}

	fclose(file);

	if (ok && info.name[0] == '\0')
		NameFromPath(path, info.name, sizeof(info.name));

	if (!ok)
		LOG("palette file: '%s' is %ld bytes and is not a palette", path.c_str(), size);

	return ok;
}

bool PaletteFile::Save(const std::string& path, const uint8_t* colors, const Info& info,
	const uint8_t* effectColors)
{
	if (colors == nullptr)
		return false;

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
		return false;

	uint32_t header[4] = {};
	memcpy(header, kPalHeader, sizeof(header));
	header[3] = effectColors != nullptr ? 2u : 1u;

	Trailer trailer = {};
	memcpy(trailer.magic, kTrailerMagic, sizeof(kTrailerMagic));
	trailer.info = info;

	bool ok =
		fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
		fwrite(colors, 1, kBytes, file) == kBytes;

	if (ok && effectColors != nullptr)
		ok = fwrite(effectColors, 1, kBytes, file) == kBytes;

	if (ok)
		ok = fwrite(&trailer, 1, sizeof(trailer), file) == sizeof(trailer);

	fclose(file);
	return ok;
}
