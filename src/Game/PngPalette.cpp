#include "Game/PngPalette.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr uint8_t kSignature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
constexpr int kEntries = 256;
constexpr int kGridSide = 16;

uint32_t ReadBigEndian32(const uint8_t* bytes)
{
	return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
		(static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
}

void WriteBigEndian32(std::vector<uint8_t>& out, uint32_t value)
{
	out.push_back(static_cast<uint8_t>(value >> 24));
	out.push_back(static_cast<uint8_t>(value >> 16));
	out.push_back(static_cast<uint8_t>(value >> 8));
	out.push_back(static_cast<uint8_t>(value));
}

uint32_t Crc32(const uint8_t* data, size_t size)
{
	static uint32_t table[256];
	static bool built = false;

	if (!built)
	{
		for (uint32_t n = 0; n < 256; ++n)
		{
			uint32_t c = n;
			for (int k = 0; k < 8; ++k)
				c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			table[n] = c;
		}
		built = true;
	}

	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < size; ++i)
		crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);

	return crc ^ 0xFFFFFFFFu;
}

uint32_t Adler32(const uint8_t* data, size_t size)
{
	uint32_t a = 1;
	uint32_t b = 0;

	for (size_t i = 0; i < size; ++i)
	{
		a = (a + data[i]) % 65521u;
		b = (b + a) % 65521u;
	}

	return (b << 16) | a;
}

void WriteChunk(std::vector<uint8_t>& out, const char* type, const uint8_t* data, size_t size)
{
	WriteBigEndian32(out, static_cast<uint32_t>(size));

	const size_t typeAt = out.size();
	out.insert(out.end(), type, type + 4);

	if (size > 0)
		out.insert(out.end(), data, data + size);

	const uint32_t crc = Crc32(out.data() + typeAt, 4 + size);
	WriteBigEndian32(out, crc);
}

std::vector<uint8_t> ZlibStore(const std::vector<uint8_t>& raw)
{
	std::vector<uint8_t> out;
	out.push_back(0x78);
	out.push_back(0x01);

	size_t at = 0;

	while (at < raw.size() || out.size() == 2)
	{
		const size_t chunk = raw.size() - at < 65535 ? raw.size() - at : 65535;
		const bool final = at + chunk >= raw.size();

		out.push_back(final ? 1 : 0);

		const uint16_t len = static_cast<uint16_t>(chunk);
		const uint16_t nlen = static_cast<uint16_t>(~len);

		out.push_back(static_cast<uint8_t>(len));
		out.push_back(static_cast<uint8_t>(len >> 8));
		out.push_back(static_cast<uint8_t>(nlen));
		out.push_back(static_cast<uint8_t>(nlen >> 8));

		out.insert(out.end(), raw.begin() + at, raw.begin() + at + chunk);

		at += chunk;

		if (chunk == 0)
			break;
	}

	const uint32_t adler = Adler32(raw.data(), raw.size());
	out.push_back(static_cast<uint8_t>(adler >> 24));
	out.push_back(static_cast<uint8_t>(adler >> 16));
	out.push_back(static_cast<uint8_t>(adler >> 8));
	out.push_back(static_cast<uint8_t>(adler));

	return out;
}

bool ReadFile(const std::string& path, std::vector<uint8_t>& outData)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
		return false;

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size <= 0)
	{
		fclose(file);
		return false;
	}

	outData.resize(static_cast<size_t>(size));
	const bool ok = fread(outData.data(), 1, outData.size(), file) == outData.size();

	fclose(file);
	return ok;
}

void FillFromPalette(const uint8_t* palette, int entries, uint8_t* outRgba)
{
	for (int i = 0; i < kEntries; ++i)
	{
		if (i < entries)
		{
			outRgba[i * 4 + 0] = palette[i * 3 + 0];
			outRgba[i * 4 + 1] = palette[i * 3 + 1];
			outRgba[i * 4 + 2] = palette[i * 3 + 2];
		}
		else
		{
			outRgba[i * 4 + 0] = 0;
			outRgba[i * 4 + 1] = 0;
			outRgba[i * 4 + 2] = 0;
		}

		outRgba[i * 4 + 3] = 255;
	}
}

}

bool PngPalette::Read(const std::string& path, uint8_t* outRgba, std::string& outError)
{
	std::vector<uint8_t> data;
	if (!ReadFile(path, data))
	{
		outError = "could not read the file";
		return false;
	}

	if (data.size() < sizeof(kSignature) || memcmp(data.data(), kSignature, sizeof(kSignature)) != 0)
	{
		outError = "not a PNG file";
		return false;
	}

	size_t at = sizeof(kSignature);

	while (at + 8 <= data.size())
	{
		const uint32_t length = ReadBigEndian32(data.data() + at);
		const uint8_t* const type = data.data() + at + 4;
		const size_t chunkData = at + 8;

		if (chunkData + length + 4 > data.size())
			break;

		if (memcmp(type, "PLTE", 4) == 0)
		{
			const int entries = static_cast<int>(length / 3) < kEntries
				? static_cast<int>(length / 3) : kEntries;

			FillFromPalette(data.data() + chunkData, entries, outRgba);
			return true;
		}

		if (memcmp(type, "IDAT", 4) == 0 || memcmp(type, "IEND", 4) == 0)
			break;

		at = chunkData + length + 4;
	}

	outError = "this PNG has no embedded palette - export it as an indexed / 8-bit image, not RGB";
	return false;
}

bool PngPalette::Write(const std::string& path, const uint8_t* rgba, std::string& outError)
{
	uint8_t ihdr[13] = {};
	ihdr[0] = 0;
	ihdr[1] = 0;
	ihdr[2] = 0;
	ihdr[3] = kGridSide;
	ihdr[4] = 0;
	ihdr[5] = 0;
	ihdr[6] = 0;
	ihdr[7] = kGridSide;
	ihdr[8] = 8;
	ihdr[9] = 3;
	ihdr[10] = 0;
	ihdr[11] = 0;
	ihdr[12] = 0;

	uint8_t plte[kEntries * 3] = {};
	for (int i = 0; i < kEntries; ++i)
	{
		plte[i * 3 + 0] = rgba[i * 4 + 0];
		plte[i * 3 + 1] = rgba[i * 4 + 1];
		plte[i * 3 + 2] = rgba[i * 4 + 2];
	}

	std::vector<uint8_t> raw;
	raw.reserve(kGridSide * (1 + kGridSide));

	for (int y = 0; y < kGridSide; ++y)
	{
		raw.push_back(0);

		for (int x = 0; x < kGridSide; ++x)
			raw.push_back(static_cast<uint8_t>(y * kGridSide + x));
	}

	const std::vector<uint8_t> idat = ZlibStore(raw);

	std::vector<uint8_t> file;
	file.insert(file.end(), kSignature, kSignature + sizeof(kSignature));
	WriteChunk(file, "IHDR", ihdr, sizeof(ihdr));
	WriteChunk(file, "PLTE", plte, sizeof(plte));
	WriteChunk(file, "IDAT", idat.data(), idat.size());
	WriteChunk(file, "IEND", nullptr, 0);

	FILE* out = nullptr;
	if (fopen_s(&out, path.c_str(), "wb") != 0 || out == nullptr)
	{
		outError = "could not create the file";
		return false;
	}

	const bool ok = fwrite(file.data(), 1, file.size(), out) == file.size();
	fclose(out);

	if (!ok)
		outError = "could not write the file";

	return ok;
}
