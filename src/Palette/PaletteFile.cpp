#include "Palette/PaletteFile.h"

#include "Core/logger.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t kPalHeader[4] = { 0x0000ffff, 1, 0, 1 };
constexpr int kPalHeaderBytes = 16;

constexpr char kTrailerMagic[8] = { 'U', 'N', 'I', '2', 'I', 'M', 'P', 'L' };
constexpr char kEffectMagic[4] = { 'U', 'I', '2', 'E' };

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

bool ReadLegacyEffectBlock(FILE* file, long size, long from, uint8_t* effectColors, bool& outHasEffect)
{
	if (size - from < PaletteFile::kBytes)
		return false;

	if (fseek(file, from, SEEK_SET) != 0)
		return false;

	if (effectColors == nullptr)
		return true;

	if (fread(effectColors, 1, PaletteFile::kBytes, file) != PaletteFile::kBytes)
		return false;

	outHasEffect = true;
	return true;
}

long ReadCompactEffectBlock(FILE* file, long size, long from, uint8_t* effectColors, bool& outHasEffect)
{
	if (fseek(file, from, SEEK_SET) != 0)
		return 0;

	char magic[4] = {};
	if (fread(magic, 1, sizeof(magic), file) != sizeof(magic))
		return 0;

	if (memcmp(magic, kEffectMagic, sizeof(kEffectMagic)) != 0)
		return 0;

	uint16_t count = 0;
	if (fread(&count, 1, sizeof(count), file) != sizeof(count))
		return 0;

	const long recordBytes = static_cast<long>(count) * 4;
	if (from + 4 + 2 + recordBytes > size)
		return 0;

	for (int i = 0; i < count; ++i)
	{
		uint8_t record[4] = {};
		if (fread(record, 1, sizeof(record), file) != sizeof(record))
			return 0;

		if (effectColors == nullptr)
			continue;

		uint8_t* const entry = effectColors + record[0] * 4;
		entry[0] = record[1];
		entry[1] = record[2];
		entry[2] = record[3];
		entry[3] = 255;
	}

	if (effectColors != nullptr && count > 0)
		outHasEffect = true;

	return 4 + 2 + recordBytes;
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

bool WriteEffectBlock(FILE* file, const uint8_t* effectColors)
{
	uint16_t count = 0;
	for (int i = 1; i < PaletteFile::kColors; ++i)
		count += effectColors[i * 4 + 3] == 255 ? 1 : 0;

	if (count == 0)
		return true;

	if (fwrite(kEffectMagic, 1, sizeof(kEffectMagic), file) != sizeof(kEffectMagic))
		return false;

	if (fwrite(&count, 1, sizeof(count), file) != sizeof(count))
		return false;

	for (int i = 1; i < PaletteFile::kColors; ++i)
	{
		if (effectColors[i * 4 + 3] != 255)
			continue;

		const uint8_t record[4] = { static_cast<uint8_t>(i), effectColors[i * 4 + 0],
			effectColors[i * 4 + 1], effectColors[i * 4 + 2] };

		if (fwrite(record, 1, sizeof(record), file) != sizeof(record))
			return false;
	}

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
	bool hasEffect = false;

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

				long after = start + kBytes;

				if (ok && count >= 2)
					after = ReadLegacyEffectBlock(file, size, after, effectColors, hasEffect)
						? after + kBytes : after;
				else if (ok)
					after += ReadCompactEffectBlock(file, size, after, effectColors, hasEffect);

				ReadTrailer(file, after, size, info);
			}
		}
	}

	fclose(file);

	if (ok && info.name[0] == '\0')
		NameFromPath(path, info.name, sizeof(info.name));

	if (!ok)
		LOG("palette file: '%s' is %ld bytes and is not a palette", path.c_str(), size);

	if (outHasEffect != nullptr)
		*outHasEffect = hasEffect;

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

	bool ok =
		fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
		fwrite(colors, 1, kBytes, file) == kBytes;

	if (ok && effectColors != nullptr)
		ok = WriteEffectBlock(file, effectColors);

	if (ok)
	{
		Trailer trailer = {};
		memcpy(trailer.magic, kTrailerMagic, sizeof(kTrailerMagic));
		trailer.info = info;

		ok = fwrite(&trailer, 1, sizeof(trailer), file) == sizeof(trailer);
	}

	fclose(file);
	return ok;
}
