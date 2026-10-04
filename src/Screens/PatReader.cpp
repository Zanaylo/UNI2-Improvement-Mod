#include "Screens/PatReader.h"

#include <cstring>

namespace {

constexpr size_t kBody = 0x20;
constexpr size_t kUnknownTag = static_cast<size_t>(-1);
constexpr uint32_t kMaxSurface = 64u * 1024u * 1024u;
constexpr int kBlockSide = 4;
constexpr uint64_t kDxt1Block = 8;
constexpr uint64_t kDxt5Block = 16;

const char* const kTopTags[] = { "P_ST", "PPST", "PGST", "VEST", "_END" };
const char* const kCutTags[] = { "PPNA", "PPNM", "PPUV", "PPCC", "PPSS", "PPPA", "PPTP", "PPPP",
	"PPTE", "PPJP", "PPED" };
const char* const kAtlasTags[] = { "PGST", "PPST", "P_ST", "VEST", "_END" };

bool Tag(const std::vector<uint8_t>& blob, size_t at, const char* name)
{
	return at + 4 <= blob.size() && memcmp(&blob[at], name, 4) == 0;
}

template <size_t N>
size_t Resync(const std::vector<uint8_t>& blob, size_t at, const char* const (&tags)[N])
{
	size_t probe = at + 4;

	while (probe + 4 <= blob.size())
	{
		for (size_t i = 0; i < N; ++i)
		{
			if (Tag(blob, probe, tags[i]))
				return probe;
		}

		++probe;
	}

	return blob.size();
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
	uint32_t value = 0;

	if (at + 4 <= blob.size())
		memcpy(&value, &blob[at], 4);

	return value;
}

float F32(const std::vector<uint8_t>& blob, size_t at)
{
	float value = 0.0f;

	if (at + 4 <= blob.size())
		memcpy(&value, &blob[at], 4);

	return value;
}

uint64_t Blocks(int width, int height, uint64_t blockBytes)
{
	const uint64_t across = static_cast<uint64_t>((width + kBlockSide - 1) / kBlockSide);
	const uint64_t down = static_cast<uint64_t>((height + kBlockSide - 1) / kBlockSide);

	return across * down * blockBytes;
}

uint64_t SurfaceBytes(const std::vector<uint8_t>& blob, size_t at, int width, int height)
{
	if (at + 4 > blob.size())
		return 0;

	if (memcmp(&blob[at], "DXT5", 4) == 0)
		return Blocks(width, height, kDxt5Block);

	if (memcmp(&blob[at], "DXT1", 4) == 0)
		return Blocks(width, height, kDxt1Block);

	const uint32_t format = U32(blob, at);
	const uint64_t pixels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

	if (format == 21 || format == 22)
		return pixels * 4;

	if (format == 23 || format == 25 || format == 26)
		return pixels * 2;

	return 0;
}

void RleDecode(const std::vector<uint8_t>& blob, size_t at, uint32_t packed, uint32_t size,
	std::vector<uint8_t>& out)
{
	out.assign(size, 0);

	size_t read = at;
	const size_t end = at + packed;
	size_t write = 0;

	while (read < end && read < blob.size() && write < size)
	{
		const uint8_t byte = blob[read];

		if (byte != 0)
		{
			out[write++] = byte;
			++read;
			continue;
		}

		if (read + 2 >= end || read + 2 >= blob.size())
			break;

		const uint8_t value = blob[read + 1];
		size_t count = blob[read + 2];

		if (count > size - write)
			count = size - write;

		memset(&out[write], value, count);
		write += count;
		read += 3;
	}
}

size_t ReadAtlas(const std::vector<uint8_t>& blob, PatReader::Document& doc, size_t at, int id)
{
	size_t streamAt = 0;
	uint32_t streamLen = 0;
	int width = 0;
	int height = 0;
	std::vector<uint8_t> dds;

	while (at + 4 <= blob.size() && !Tag(blob, at, "PGED"))
	{
		if (Tag(blob, at, "PGNM"))
		{
			at += 4 + 0x20;
			continue;
		}

		if (!Tag(blob, at, "PGT2"))
		{
			at += 8;
			continue;
		}

		const uint32_t packed = U32(blob, at + 4);
		width = I32(blob, at + 8);
		height = I32(blob, at + 12);

		if (width <= 0 || height <= 0)
			break;

		const uint64_t surface = SurfaceBytes(blob, at + 16, width, height);

		if (surface == 0 || surface + 128 > kMaxSurface)
			break;

		if (packed == surface + 128)
		{
			streamAt = at + 28;
			streamLen = static_cast<uint32_t>(surface + 128);

			if (streamAt + streamLen <= blob.size())
				dds.assign(blob.begin() + streamAt, blob.begin() + streamAt + streamLen);

			break;
		}

		streamLen = U32(blob, at + 36);

		const uint32_t plainLen = U32(blob, at + 40);
		streamAt = at + 44;

		if (plainLen > 128 && plainLen <= kMaxSurface)
			RleDecode(blob, streamAt, streamLen, plainLen, dds);

		break;
	}

	if (!dds.empty())
	{
		doc.atlasData.push_back(std::move(dds));

		PatReader::Atlas atlas = {};
		atlas.id = id;
		atlas.width = width;
		atlas.height = height;
		atlas.dds = doc.atlasData.back().data();
		atlas.ddsSize = doc.atlasData.back().size();
		doc.atlases.push_back(atlas);
	}

	const size_t after = (streamAt != 0 && streamLen != 0) ? streamAt + streamLen : at;

	return Resync(blob, after, kAtlasTags);
}

size_t ReadCutOut(const std::vector<uint8_t>& blob, PatReader::Document& doc, size_t at, int id)
{
	PatReader::PartRecord record = {};
	record.part.id = id;

	while (at + 4 <= blob.size())
	{
		if (Tag(blob, at, "PPED"))
		{
			at += 4;
			break;
		}

		if (Tag(blob, at, "PPNA"))
		{
			const size_t length = blob[at + 4];

			if (at + 5 + length <= blob.size())
				record.name.assign(reinterpret_cast<const char*>(&blob[at + 5]), length);

			const size_t zero = record.name.find(static_cast<char>(0));

			if (zero != std::string::npos)
				record.name.resize(zero);

			at += 5 + length;
			continue;
		}

		if (Tag(blob, at, "PPNM"))
		{
			at += 4 + 0x20;
			continue;
		}

		if (Tag(blob, at, "PPUV"))
		{
			record.part.u = I32(blob, at + 4);
			record.part.v = I32(blob, at + 8);
			record.part.w = I32(blob, at + 12);
			record.part.h = I32(blob, at + 16);
			at += 20;
			continue;
		}

		if (Tag(blob, at, "PPCC"))
		{
			record.part.pivotX = I32(blob, at + 4);
			record.part.pivotY = I32(blob, at + 8);
			at += 12;
			continue;
		}

		if (Tag(blob, at, "PPSS"))
		{
			record.part.width = I32(blob, at + 4);
			record.part.height = I32(blob, at + 8);
			at += 12;
			continue;
		}

		if (Tag(blob, at, "PPTP"))
		{
			record.part.atlas = I32(blob, at + 4);
			at += 8;
			continue;
		}

		if (Tag(blob, at, "PPPA") || Tag(blob, at, "PPTE") || Tag(blob, at, "PPPP"))
		{
			at += 8;
			continue;
		}

		if (Tag(blob, at, "PPJP"))
		{
			at += 12;
			continue;
		}

		at = Resync(blob, at, kCutTags);
	}

	doc.parts[id] = std::move(record);

	return at;
}

size_t SpriteTagSize(const std::vector<uint8_t>& blob, size_t at)
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

void ReadSpriteTag(const std::vector<uint8_t>& blob, size_t at, PatReader::Sprite& sprite)
{
	if (Tag(blob, at, "PRID"))
		sprite.part = I32(blob, at + 4);
	else if (Tag(blob, at, "PRXY"))
	{
		sprite.x = I32(blob, at + 4);
		sprite.y = I32(blob, at + 8);
	}
	else if (Tag(blob, at, "PRZM"))
	{
		sprite.zoomX = F32(blob, at + 4);
		sprite.zoomY = F32(blob, at + 8);
	}
	else if (Tag(blob, at, "PRCL"))
		sprite.tint = U32(blob, at + 4);
	else if (Tag(blob, at, "PRPR"))
		sprite.priority = I32(blob, at + 4);
	else if (Tag(blob, at, "PRAL"))
		sprite.blend = blob[at + 4];
	else if (Tag(blob, at, "PRA3"))
	{
		sprite.pitch = F32(blob, at + 8);
		sprite.yaw = F32(blob, at + 12);
		sprite.turns = F32(blob, at + 16);
	}
}

size_t ReadPattern(const std::vector<uint8_t>& blob, PatReader::Document& doc, size_t at)
{
	if (!Tag(blob, at, "PANA"))
		return blob.size();

	const size_t length = blob[at + 4];

	doc.patterns.push_back(PatReader::Pattern());
	PatReader::Pattern& pattern = doc.patterns.back();

	if (at + 5 + length <= blob.size())
		pattern.name.assign(reinterpret_cast<const char*>(&blob[at + 5]), length);

	const size_t zero = pattern.name.find(static_cast<char>(0));

	if (zero != std::string::npos)
		pattern.name.resize(zero);

	at += 5 + length;

	PatReader::Sprite* sprite = nullptr;

	while (at + 4 <= blob.size())
	{
		if (Tag(blob, at, "P_ED"))
			return at + 4;

		const size_t payload = SpriteTagSize(blob, at);

		if (payload == kUnknownTag)
		{
			++at;
			continue;
		}

		if (Tag(blob, at, "PRST"))
		{
			PatReader::Sprite made = {};
			made.id = I32(blob, at + 4);
			made.part = -1;
			made.zoomX = 1.0f;
			made.zoomY = 1.0f;
			made.tint = 0xFFFFFFFF;

			pattern.sprites.push_back(made);
			sprite = &pattern.sprites.back();
			at += 4 + payload;
			continue;
		}

		if (sprite != nullptr)
			ReadSpriteTag(blob, at, *sprite);

		at += 4 + payload;
	}

	return blob.size();
}

size_t ReadPatterns(const std::vector<uint8_t>& blob, PatReader::Document& doc)
{
	size_t at = kBody;

	if (Tag(blob, at, "_STR"))
		at += 4;

	while (at + 8 <= blob.size() && Tag(blob, at, "P_ST"))
		at = ReadPattern(blob, doc, at + 8);

	return at;
}

std::string Trimmed(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t");

	if (first == std::string::npos)
		return std::string();

	return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

}

void PatReader::Read(const std::vector<uint8_t>& blob, Document& out)
{
	out = Document();

	size_t at = ReadPatterns(blob, out);

	while (at + 8 <= blob.size())
	{
		if (Tag(blob, at, "_END"))
			break;

		if (Tag(blob, at, "PPST"))
		{
			at = ReadCutOut(blob, out, at + 8, I32(blob, at + 4));
			continue;
		}

		if (Tag(blob, at, "PGST"))
		{
			at = ReadAtlas(blob, out, at + 8, I32(blob, at + 4));
			continue;
		}

		at = Resync(blob, at, kTopTags);
	}
}

const PatReader::Pattern* PatReader::Find(const Document& document, const std::string& name)
{
	const std::string wanted = Trimmed(name);
	const Pattern* trimmed = nullptr;

	for (const Pattern& pattern : document.patterns)
	{
		if (pattern.name == name)
			return &pattern;

		if (trimmed == nullptr && Trimmed(pattern.name) == wanted)
			trimmed = &pattern;
	}

	return trimmed;
}

bool PatReader::PartOf(const Document& document, int id, Part& out)
{
	const auto found = document.parts.find(id);

	if (found == document.parts.end())
		return false;

	out = found->second.part;
	out.name = found->second.name.c_str();

	return true;
}

const PatReader::Atlas* PatReader::AtlasOf(const Document& document, int id)
{
	for (const Atlas& atlas : document.atlases)
	{
		if (atlas.id == id)
			return &atlas;
	}

	return nullptr;
}
