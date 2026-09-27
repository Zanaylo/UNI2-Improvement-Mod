#include "Screens/PatWriter.h"

#include <cstring>

namespace {

constexpr const char* kHeader = "PAniDataFile";
constexpr size_t kBody = 0x20;
constexpr size_t kAtlasName = 40;
constexpr uint32_t kFormatArgb = 21;
constexpr uint32_t kDdsHeader = 124;
constexpr uint32_t kDdsFlags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000;
constexpr uint32_t kDdsPixelFormat = 32;
constexpr uint32_t kDdsRgbAlpha = 0x1 | 0x40;
constexpr uint32_t kDdsBits = 32;
constexpr uint32_t kDdsTexture = 0x1000;
constexpr size_t kDdsReserved = 44;
constexpr size_t kDdsTail = 16;
constexpr size_t kDdsSize = 128;
constexpr size_t kBytesPerPixel = 4;

class Out
{
public:
	void Tag(const char* name) { Bytes(name, 4); }
	void Byte(uint8_t value) { m_blob.push_back(value); }

	void Dword(uint32_t value)
	{
		for (int shift = 0; shift < 32; shift += 8)
			m_blob.push_back(static_cast<uint8_t>(value >> shift));
	}

	void Int(int value) { Dword(static_cast<uint32_t>(value)); }

	void Word(uint16_t value)
	{
		m_blob.push_back(static_cast<uint8_t>(value));
		m_blob.push_back(static_cast<uint8_t>(value >> 8));
	}

	void Float(float value)
	{
		uint32_t raw = 0;
		memcpy(&raw, &value, 4);
		Dword(raw);
	}

	void Bytes(const void* data, size_t size)
	{
		const uint8_t* const bytes = static_cast<const uint8_t*>(data);
		m_blob.insert(m_blob.end(), bytes, bytes + size);
	}

	void Zeros(size_t size) { m_blob.insert(m_blob.end(), size, 0); }

	void Name(const char* tag, const std::string& text)
	{
		Tag(tag);
		Byte(static_cast<uint8_t>(text.size()));
		Bytes(text.data(), text.size());
	}

	std::vector<uint8_t>& Blob() { return m_blob; }

private:
	std::vector<uint8_t> m_blob;
};

void WriteSprite(Out& out, const PatWriter::Sprite& sprite)
{
	out.Tag("PRST");
	out.Int(sprite.id);
	out.Tag("PRXY");
	out.Int(sprite.x);
	out.Int(sprite.y);
	out.Tag("PRAL");
	out.Byte(sprite.additive);
	out.Tag("PRFL");
	out.Byte(0);
	out.Tag("PRZM");
	out.Float(sprite.zoom[0]);
	out.Float(sprite.zoom[1]);
	out.Tag("PRPR");
	out.Int(sprite.priority);
	out.Tag("PRID");
	out.Int(sprite.part);

	if (sprite.tinted)
	{
		out.Tag("PRCL");
		out.Byte(sprite.tint[2]);
		out.Byte(sprite.tint[1]);
		out.Byte(sprite.tint[0]);
		out.Byte(sprite.tint[3]);
	}

	if (sprite.turned)
	{
		out.Tag("PRA3");
		out.Int(0);
		out.Float(0.0f);
		out.Float(0.0f);
		out.Float(sprite.turn);
	}

	out.Tag("PRED");
}

void WriteCutout(Out& out, const PatWriter::Cutout& cut, const PatWriter::Atlas& atlas)
{
	out.Tag("PPST");
	out.Int(cut.id);
	out.Name("PPNA", cut.name);
	out.Tag("PPCC");
	out.Int(cut.pivot[0]);
	out.Int(cut.pivot[1]);
	out.Tag("PPUV");

	for (const int value : cut.uv)
		out.Int(value);

	out.Tag("PPSS");
	out.Int(cut.size[0]);
	out.Int(cut.size[1]);
	out.Tag("PPTP");
	out.Int(0);
	out.Tag("PPTE");
	out.Word(static_cast<uint16_t>(atlas.width));
	out.Word(static_cast<uint16_t>(atlas.height));
	out.Tag("PPED");
}

void WriteDds(Out& out, const PatWriter::Atlas& atlas)
{
	out.Bytes("DDS ", 4);
	out.Dword(kDdsHeader);
	out.Dword(kDdsFlags);
	out.Int(atlas.height);
	out.Int(atlas.width);
	out.Dword(static_cast<uint32_t>(atlas.width * kBytesPerPixel));
	out.Dword(0);
	out.Dword(0);
	out.Zeros(kDdsReserved);
	out.Dword(kDdsPixelFormat);
	out.Dword(kDdsRgbAlpha);
	out.Dword(0);
	out.Dword(kDdsBits);
	out.Dword(0x00ff0000u);
	out.Dword(0x0000ff00u);
	out.Dword(0x000000ffu);
	out.Dword(0xff000000u);
	out.Dword(kDdsTexture);
	out.Zeros(kDdsTail);

	for (size_t at = 0; at + 3 < atlas.rgba.size(); at += 4)
	{
		out.Byte(atlas.rgba[at + 2]);
		out.Byte(atlas.rgba[at + 1]);
		out.Byte(atlas.rgba[at]);
		out.Byte(atlas.rgba[at + 3]);
	}
}

void WriteAtlas(Out& out, const PatWriter::Atlas& atlas)
{
	const uint32_t surface = static_cast<uint32_t>(atlas.width * atlas.height * kBytesPerPixel);

	out.Tag("PGST");
	out.Int(0);
	out.Tag("PGNM");

	const size_t named = atlas.name.size() < kAtlasName ? atlas.name.size() : kAtlasName;
	out.Bytes(atlas.name.data(), named);
	out.Zeros(kAtlasName - named);

	out.Tag("PGT2");
	out.Dword(surface + static_cast<uint32_t>(kDdsSize));
	out.Int(atlas.width);
	out.Int(atlas.height);
	out.Dword(kFormatArgb);
	out.Zeros(8);
	WriteDds(out, atlas);
	out.Tag("PGED");
}

}

std::vector<uint8_t> PatWriter::Build(const std::vector<Pattern>& patterns,
	const std::vector<Cutout>& cutouts, const Atlas& atlas)
{
	Out out;
	out.Bytes(kHeader, strlen(kHeader));
	out.Zeros(kBody - strlen(kHeader));
	out.Tag("_STR");

	for (size_t index = 0; index < patterns.size(); ++index)
	{
		out.Tag("P_ST");
		out.Int(static_cast<int>(index));
		out.Name("PANA", patterns[index].name);

		for (const Sprite& sprite : patterns[index].sprites)
			WriteSprite(out, sprite);

		out.Tag("P_ED");
	}

	for (const Cutout& cut : cutouts)
		WriteCutout(out, cut, atlas);

	WriteAtlas(out, atlas);
	out.Tag("_END");

	return out.Blob();
}
