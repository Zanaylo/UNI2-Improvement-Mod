#include "Screens/PatEdit.h"

#include "Screens/PatBytes.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr size_t kBody = 0x20;
constexpr size_t kTag = 4;
constexpr size_t kNameBlock = 0x20;
constexpr size_t kPlainOffset = 28;
constexpr size_t kPackedLengthOffset = 36;
constexpr size_t kPlainLengthOffset = 40;
constexpr size_t kPackedOffset = 44;
constexpr size_t kUnknownTag = static_cast<size_t>(-1);
constexpr uint32_t kFormatArgb = 21;
constexpr uint32_t kFormatTrailerA = 4;
constexpr uint32_t kFormatTrailerB = 3;
constexpr int kBytesPerPixel = 4;
constexpr int kUvUnits = 256;

bool Tag(const std::vector<uint8_t>& blob, size_t at, const char* name)
{
	return at + kTag <= blob.size() && memcmp(&blob[at], name, kTag) == 0;
}

int32_t I32(const std::vector<uint8_t>& blob, size_t at)
{
	int32_t value = 0;

	if (at + 4 <= blob.size())
		memcpy(&value, &blob[at], 4);

	return value;
}

uint32_t U32(const std::vector<uint8_t>& blob, size_t at)
{
	return static_cast<uint32_t>(I32(blob, at));
}

std::string ShortName(const std::vector<uint8_t>& blob, size_t at, size_t& after)
{
	const size_t length = at < blob.size() ? blob[at] : 0;
	after = at + 1 + length;

	if (after > blob.size())
		return std::string();

	std::string name(reinterpret_cast<const char*>(&blob[at + 1]), length);
	const size_t zero = name.find('\0');

	if (zero != std::string::npos)
		name.resize(zero);

	return name;
}

size_t SpriteTagPayload(const std::vector<uint8_t>& blob, size_t at)
{
	if (Tag(blob, at, "PRXY") || Tag(blob, at, "PRZM"))
		return 8;

	if (Tag(blob, at, "PRST") || Tag(blob, at, "PRID") || Tag(blob, at, "PRPR") ||
		Tag(blob, at, "PRCL") || Tag(blob, at, "PRSP"))
	{
		return 4;
	}

	if (Tag(blob, at, "PRFL") || Tag(blob, at, "PRAL") || Tag(blob, at, "PRRV"))
		return 1;

	if (Tag(blob, at, "PRA3"))
		return 16;

	if (Tag(blob, at, "PRED") || Tag(blob, at, "APRC") || Tag(blob, at, "LPRA"))
		return 0;

	return kUnknownTag;
}

size_t PatternEnd(const std::vector<uint8_t>& blob, size_t at)
{
	while (at + kTag <= blob.size())
	{
		if (Tag(blob, at, "P_ED"))
			return at + kTag;

		const size_t payload = SpriteTagPayload(blob, at);
		at += payload == kUnknownTag ? 1 : kTag + payload;
	}

	return 0;
}

bool ReadPattern(const std::vector<uint8_t>& blob, size_t at, PatEdit::Pattern& out)
{
	if (!Tag(blob, at + 8, "PANA"))
		return false;

	size_t afterName = 0;
	out.begin = at;
	out.index = I32(blob, at + 4);
	out.name = ShortName(blob, at + 12, afterName);
	out.end = PatternEnd(blob, afterName);

	return out.end != 0;
}

size_t PartTagStep(const std::vector<uint8_t>& blob, size_t at, PatEdit::Part& part)
{
	if (Tag(blob, at, "PPNA"))
	{
		size_t after = 0;
		part.name = ShortName(blob, at + kTag, after);
		return after - at;
	}

	if (Tag(blob, at, "PPNM"))
		return kTag + kNameBlock;

	if (Tag(blob, at, "PPUV"))
	{
		for (int i = 0; i < 4; ++i)
			part.uv[i] = I32(blob, at + kTag + 4 * i);

		return kTag + 16;
	}

	if (Tag(blob, at, "PPCC") || Tag(blob, at, "PPSS"))
	{
		int* const pair = Tag(blob, at, "PPCC") ? part.pivot : part.size;
		pair[0] = I32(blob, at + kTag);
		pair[1] = I32(blob, at + kTag + 4);
		return kTag + 8;
	}

	if (Tag(blob, at, "PPTP"))
	{
		part.atlas = I32(blob, at + kTag);
		return kTag + 4;
	}

	if (Tag(blob, at, "PPPA") || Tag(blob, at, "PPTE") || Tag(blob, at, "PPPP"))
		return kTag + 4;

	if (Tag(blob, at, "PPJP"))
		return kTag + 8;

	return 1;
}

size_t ReadPart(const std::vector<uint8_t>& blob, size_t at, PatEdit::Part& out)
{
	out.id = I32(blob, at + kTag);
	at += kTag + 4;

	while (at + kTag <= blob.size())
	{
		if (Tag(blob, at, "PPED"))
			return at + kTag;

		at += PartTagStep(blob, at, out);
	}

	return 0;
}

size_t SurfaceBytes(const std::vector<uint8_t>& blob, size_t at, int width, int height)
{
	const size_t pixels = static_cast<size_t>(width) * height;

	if (Tag(blob, at, "DXT5"))
		return pixels;

	if (Tag(blob, at, "DXT1"))
		return pixels / 2;

	const uint32_t format = U32(blob, at);

	if (format == 21 || format == 22)
		return pixels * 4;

	if (format == 23 || format == 25 || format == 26)
		return pixels * 2;

	return 0;
}

bool ReadSurface(const std::vector<uint8_t>& blob, size_t at, PatEdit::Atlas& out)
{
	out.width = I32(blob, at + 8);
	out.height = I32(blob, at + 12);

	const size_t surface = SurfaceBytes(blob, at + 16, out.width, out.height);

	if (surface == 0 || out.width <= 0 || out.height <= 0)
		return false;

	const uint32_t packedField = U32(blob, at + 4);
	out.packed = packedField != surface + DdsImage::kHeaderBytes;

	if (!out.packed)
	{
		out.payload = at + kPlainOffset;
		out.payloadBytes = surface + DdsImage::kHeaderBytes;
		out.plainBytes = out.payloadBytes;
		return out.payload + out.payloadBytes <= blob.size();
	}

	out.payload = at + kPackedOffset;
	out.payloadBytes = U32(blob, at + kPackedLengthOffset);
	out.plainBytes = U32(blob, at + kPlainLengthOffset);

	return out.payload + out.payloadBytes <= blob.size() && out.plainBytes > DdsImage::kHeaderBytes;
}

size_t ReadAtlas(const std::vector<uint8_t>& blob, size_t at, PatEdit::Atlas& out)
{
	out.begin = at;
	out.id = I32(blob, at + kTag);
	at += kTag + 4;

	while (at + kTag <= blob.size())
	{
		if (Tag(blob, at, "PGED"))
			return at + kTag;

		if (Tag(blob, at, "PGNM"))
		{
			const char* const name = reinterpret_cast<const char*>(&blob[at + kTag]);
			out.name.assign(name, strnlen(name, kNameBlock));
			at += kTag + kNameBlock;
			continue;
		}

		if (Tag(blob, at, "PGTE"))
		{
			at += kTag + 4;
			continue;
		}

		if (!Tag(blob, at, "PGT2"))
		{
			++at;
			continue;
		}

		if (!ReadSurface(blob, at, out))
			return 0;

		at = out.payload + out.payloadBytes;
	}

	return 0;
}

void Unpack(const std::vector<uint8_t>& blob, const PatEdit::Atlas& atlas, std::vector<uint8_t>& out)
{
	out.assign(atlas.plainBytes, 0);

	size_t read = atlas.payload;
	const size_t end = atlas.payload + atlas.payloadBytes;
	size_t write = 0;

	while (read < end && write < out.size())
	{
		const uint8_t byte = blob[read];

		if (byte != 0)
		{
			out[write++] = byte;
			++read;
			continue;
		}

		if (read + 2 >= end)
			break;

		const size_t count = std::min<size_t>(blob[read + 2], out.size() - write);
		memset(&out[write], blob[read + 1], count);
		write += count;
		read += 3;
	}
}

size_t ShapesEnd(const std::vector<uint8_t>& blob, size_t at)
{
	for (at += kTag; at + kTag <= blob.size(); ++at)
	{
		if (Tag(blob, at, "VEED"))
			return at + kTag;
	}

	return 0;
}

void Append(std::vector<uint8_t>& out, const std::vector<uint8_t>& from, size_t begin, size_t end)
{
	out.insert(out.end(), from.begin() + begin, from.begin() + end);
}

std::vector<uint8_t> RemappedSprites(const std::vector<uint8_t>& blob, size_t at, size_t end,
	const std::map<int, int>& partMap)
{
	std::vector<uint8_t> out(blob.begin() + at, blob.begin() + end);

	for (size_t cursor = 0; cursor + kTag <= out.size();)
	{
		const size_t payload = SpriteTagPayload(out, cursor);

		if (payload == kUnknownTag)
		{
			++cursor;
			continue;
		}

		if (Tag(out, cursor, "PRID"))
		{
			const auto mapped = partMap.find(I32(out, cursor + kTag));

			if (mapped != partMap.end())
				memcpy(&out[cursor + kTag], &mapped->second, 4);
		}

		cursor += kTag + payload;
	}

	return out;
}

}

bool PatEdit::Survey(const std::vector<uint8_t>& pat, Layout& out)
{
	out = Layout();

	if (pat.size() < kBody || memcmp(pat.data(), "PAniDataFile", 12) != 0)
		return false;

	size_t at = kBody;

	if (Tag(pat, at, "_STR"))
		at += kTag;

	while (Tag(pat, at, "P_ST"))
	{
		Pattern pattern;

		if (!ReadPattern(pat, at, pattern))
			return false;

		out.patterns.push_back(pattern);
		at = pattern.end;
	}

	out.partsBegin = at;

	while (Tag(pat, at, "PPST"))
	{
		Part part;
		const size_t next = ReadPart(pat, at, part);

		if (next == 0)
			return false;

		out.parts.push_back(part);
		at = next;
	}

	out.partsEnd = at;

	while (Tag(pat, at, "VEST"))
	{
		const size_t shapesEnd = ShapesEnd(pat, at);

		if (shapesEnd == 0)
			return false;

		at = shapesEnd;
	}

	out.atlasesBegin = at;

	while (Tag(pat, at, "PGST"))
	{
		Atlas atlas;
		const size_t next = ReadAtlas(pat, at, atlas);

		if (next == 0)
			return false;

		atlas.end = next;
		out.atlases.push_back(atlas);
		at = next;
	}

	out.end = at;

	return Tag(pat, at, "_END");
}

const PatEdit::Pattern* PatEdit::FindPattern(const Layout& layout, const std::string& name)
{
	for (const Pattern& pattern : layout.patterns)
	{
		if (pattern.name == name)
			return &pattern;
	}

	return nullptr;
}

const PatEdit::Part* PatEdit::FindPart(const Layout& layout, int id)
{
	for (const Part& part : layout.parts)
	{
		if (part.id == id)
			return &part;
	}

	return nullptr;
}

const PatEdit::Part* PatEdit::FindPartNamed(const Layout& layout, const std::string& name)
{
	for (const Part& part : layout.parts)
	{
		if (part.name == name)
			return &part;
	}

	return nullptr;
}

const PatEdit::Atlas* PatEdit::FindAtlas(const Layout& layout, int id)
{
	for (const Atlas& atlas : layout.atlases)
	{
		if (atlas.id == id)
			return &atlas;
	}

	return nullptr;
}

int PatEdit::NextPatternIndex(const Layout& layout)
{
	int highest = -1;

	for (const Pattern& pattern : layout.patterns)
		highest = std::max(highest, pattern.index);

	return highest + 1;
}

int PatEdit::NextPartId(const Layout& layout)
{
	int highest = -1;

	for (const Part& part : layout.parts)
		highest = std::max(highest, part.id);

	return highest + 1;
}

int PatEdit::NextAtlasId(const Layout& layout)
{
	int highest = -1;

	for (const Atlas& atlas : layout.atlases)
		highest = std::max(highest, atlas.id);

	return highest + 1;
}

bool PatEdit::AtlasBytes(const std::vector<uint8_t>& pat, const Atlas& atlas, std::vector<uint8_t>& out)
{
	if (atlas.packed)
	{
		Unpack(pat, atlas, out);
		return !out.empty();
	}

	if (atlas.payload + atlas.payloadBytes > pat.size())
		return false;

	out.assign(pat.begin() + atlas.payload, pat.begin() + atlas.payload + atlas.payloadBytes);
	return true;
}

bool PatEdit::DecodeAtlas(const std::vector<uint8_t>& pat, const Atlas& atlas, DdsImage::Image& out)
{
	std::vector<uint8_t> dds;

	return AtlasBytes(pat, atlas, dds) && DdsImage::Decode(dds, out);
}

ImageOps::Rect PatEdit::PartRect(const Part& part, int atlasWidth, int atlasHeight)
{
	return ImageOps::Rect{ part.uv[0] * atlasWidth / kUvUnits, part.uv[1] * atlasHeight / kUvUnits,
		part.uv[2] * atlasWidth / kUvUnits, part.uv[3] * atlasHeight / kUvUnits };
}

bool PatEdit::CutPart(const std::vector<uint8_t>& pat, const Layout& layout, const Part& part,
	DdsImage::Image& out)
{
	const Atlas* const atlas = FindAtlas(layout, part.atlas);
	DdsImage::Image surface;

	if (atlas == nullptr || !DecodeAtlas(pat, *atlas, surface))
		return false;

	out = ImageOps::Crop(surface, PartRect(part, surface.width, surface.height));
	return true;
}

std::vector<uint8_t> PatEdit::ClonePattern(const std::vector<uint8_t>& pat, const Pattern& pattern,
	int index, const std::string& name, const std::map<int, int>& partMap)
{
	size_t afterName = 0;
	ShortName(pat, pattern.begin + 12, afterName);

	PatBytes out;
	out.Tag("P_ST");
	out.Int(index);
	out.Name("PANA", name);

	const std::vector<uint8_t> sprites = RemappedSprites(pat, afterName, pattern.end, partMap);
	out.Bytes(sprites.data(), sprites.size());

	return out.Blob();
}

std::vector<uint8_t> PatEdit::PartBlock(const Part& part, int atlasWidth, int atlasHeight)
{
	PatBytes out;
	out.Tag("PPST");
	out.Int(part.id);
	out.Name("PPNA", part.name);
	out.Tag("PPCC");
	out.Int(part.pivot[0]);
	out.Int(part.pivot[1]);
	out.Tag("PPUV");

	for (const int value : part.uv)
		out.Int(value);

	out.Tag("PPSS");
	out.Int(part.size[0]);
	out.Int(part.size[1]);
	out.Tag("PPTP");
	out.Int(part.atlas);
	out.Tag("PPTE");
	out.Word(static_cast<uint16_t>(atlasWidth));
	out.Word(static_cast<uint16_t>(atlasHeight));
	out.Tag("PPED");

	return out.Blob();
}

std::vector<uint8_t> PatEdit::AtlasBlock(int id, const std::string& name, const DdsImage::Image& image)
{
	const std::vector<uint8_t> dds = DdsImage::EncodeArgb(image);

	PatBytes out;
	out.Tag("PGST");
	out.Int(id);
	out.Tag("PGNM");
	out.Padded(name, kNameBlock);
	out.Tag("PGTE");
	out.Word(static_cast<uint16_t>(image.width));
	out.Word(static_cast<uint16_t>(image.height));
	out.Tag("PGT2");
	out.Dword(static_cast<uint32_t>(image.width * image.height * kBytesPerPixel +
		DdsImage::kHeaderBytes));
	out.Int(image.width);
	out.Int(image.height);
	out.Dword(kFormatArgb);
	out.Dword(kFormatTrailerA);
	out.Dword(kFormatTrailerB);
	out.Bytes(dds.data(), dds.size());
	out.Tag("PGED");

	return out.Blob();
}

std::vector<uint8_t> PatEdit::Rebuild(const std::vector<uint8_t>& pat, const Layout& layout,
	const Additions& additions)
{
	std::vector<uint8_t> out;
	out.reserve(pat.size() + additions.patterns.size() + additions.parts.size() +
		additions.atlases.size());

	Append(out, pat, 0, layout.partsBegin);
	out.insert(out.end(), additions.patterns.begin(), additions.patterns.end());

	Append(out, pat, layout.partsBegin, layout.partsEnd);
	out.insert(out.end(), additions.parts.begin(), additions.parts.end());
	Append(out, pat, layout.partsEnd, layout.atlasesBegin);

	for (const Atlas& atlas : layout.atlases)
	{
		const auto replaced = additions.replacedAtlases.find(atlas.id);

		if (replaced == additions.replacedAtlases.end())
		{
			Append(out, pat, atlas.begin, atlas.end);
			continue;
		}

		out.insert(out.end(), replaced->second.begin(), replaced->second.end());
	}

	out.insert(out.end(), additions.atlases.begin(), additions.atlases.end());
	Append(out, pat, layout.end, pat.size());

	return out;
}
