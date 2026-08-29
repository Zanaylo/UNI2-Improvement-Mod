#include "Screens/PatFile.h"

#include "Core/logger.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr size_t kBody = 0x20;
constexpr size_t kUnknownTag = static_cast<size_t>(-1);
constexpr uint32_t kMaxSurface = 64u * 1024u * 1024u;

const char* const kTopTags[] = { "P_ST", "PPST", "PGST", "VEST", "_END" };
const char* const kCutTags[] = { "PPNA", "PPNM", "PPUV", "PPCC", "PPSS", "PPPA", "PPTP", "PPPP",
	"PPTE", "PPJP", "PPED" };
const char* const kAtlasTags[] = { "PGST", "PPST", "P_ST", "VEST", "_END" };

struct PartRecord
{
	PatFile::Part part;
	std::string name;
};

struct Pattern
{
	std::string name;
	std::vector<PatFile::Sprite> sprites;
};

struct Document
{
	std::string path;
	std::vector<uint8_t> blob;
	std::vector<PatFile::Atlas> atlases;
	std::vector<std::vector<uint8_t>> atlasData;
	std::unordered_map<int, PartRecord> parts;
	std::vector<Pattern> patterns;
};

std::vector<Document*> g_documents;
char g_status[192] = "nothing loaded";

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

int FormatBytes(const std::vector<uint8_t>& blob, size_t at)
{
	if (at + 4 > blob.size())
		return 0;

	if (memcmp(&blob[at], "DXT5", 4) == 0)
		return 1;

	const uint32_t format = U32(blob, at);

	if (format == 21 || format == 22)
		return 4;

	if (format == 23 || format == 25 || format == 26)
		return 2;

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

size_t ReadAtlas(Document& doc, size_t at, int id)
{
	const std::vector<uint8_t>& blob = doc.blob;
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

		const int perPixel = FormatBytes(blob, at + 16);

		if (perPixel == 0 || width <= 0 || height <= 0)
			break;

		const uint64_t surface = static_cast<uint64_t>(width) * height * perPixel;

		if (surface + 128 > kMaxSurface)
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

		PatFile::Atlas atlas = {};
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

size_t ReadCutOut(Document& doc, size_t at, int id)
{
	const std::vector<uint8_t>& blob = doc.blob;

	PartRecord record = {};
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

size_t ReadPattern(Document& doc, size_t at)
{
	const std::vector<uint8_t>& blob = doc.blob;

	if (!Tag(blob, at, "PANA"))
		return blob.size();

	const size_t length = blob[at + 4];

	doc.patterns.push_back(Pattern());
	Pattern& pattern = doc.patterns.back();

	if (at + 5 + length <= blob.size())
		pattern.name.assign(reinterpret_cast<const char*>(&blob[at + 5]), length);

	const size_t zero = pattern.name.find(static_cast<char>(0));

	if (zero != std::string::npos)
		pattern.name.resize(zero);

	at += 5 + length;

	PatFile::Sprite* sprite = nullptr;

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
			PatFile::Sprite made = {};
			made.part = -1;
			made.zoomX = 1.0f;
			made.zoomY = 1.0f;
			made.tint = 0xFFFFFFFF;

			pattern.sprites.push_back(made);
			sprite = &pattern.sprites.back();
			at += 4 + payload;
			continue;
		}

		if (sprite == nullptr)
		{
			at += 4 + payload;
			continue;
		}

		if (Tag(blob, at, "PRID"))
			sprite->part = I32(blob, at + 4);
		else if (Tag(blob, at, "PRXY"))
		{
			sprite->x = I32(blob, at + 4);
			sprite->y = I32(blob, at + 8);
		}
		else if (Tag(blob, at, "PRZM"))
		{
			sprite->zoomX = F32(blob, at + 4);
			sprite->zoomY = F32(blob, at + 8);
		}
		else if (Tag(blob, at, "PRCL"))
			sprite->tint = U32(blob, at + 4);
		else if (Tag(blob, at, "PRPR"))
			sprite->priority = I32(blob, at + 4);
		else if (Tag(blob, at, "PRAL"))
			sprite->blend = blob[at + 4];
		else if (Tag(blob, at, "PRA3"))
			sprite->turns = F32(blob, at + 16);

		at += 4 + payload;
	}

	return blob.size();
}

size_t ReadPatterns(Document& doc)
{
	const std::vector<uint8_t>& blob = doc.blob;
	size_t at = kBody;

	if (Tag(blob, at, "_STR"))
		at += 4;

	while (at + 8 <= blob.size() && Tag(blob, at, "P_ST"))
		at = ReadPattern(doc, at + 8);

	return at;
}

PatFile::Handle Adopt(Document* doc)
{
	size_t at = ReadPatterns(*doc);

	while (at + 8 <= doc->blob.size())
	{
		if (Tag(doc->blob, at, "_END"))
			break;

		if (Tag(doc->blob, at, "PPST"))
		{
			at = ReadCutOut(*doc, at + 8, I32(doc->blob, at + 4));
			continue;
		}

		if (Tag(doc->blob, at, "PGST"))
		{
			at = ReadAtlas(*doc, at + 8, I32(doc->blob, at + 4));
			continue;
		}

		at = Resync(doc->blob, at, kTopTags);
	}

	g_documents.push_back(doc);

	sprintf_s(g_status, "%d atlas(es), %d part(s), %d pattern(s)",
		static_cast<int>(doc->atlases.size()), static_cast<int>(doc->parts.size()),
		static_cast<int>(doc->patterns.size()));

	LOG("PatFile: %s - %s", doc->path.c_str(), g_status);

	return static_cast<PatFile::Handle>(g_documents.size() - 1);
}

Document* Get(PatFile::Handle handle)
{
	if (handle < 0 || handle >= static_cast<int>(g_documents.size()))
		return nullptr;

	return g_documents[handle];
}

}

namespace {

PatFile::Handle Find(const char* name)
{
	for (size_t i = 0; i < g_documents.size(); ++i)
	{
		if (g_documents[i] != nullptr && g_documents[i]->path == name)
			return static_cast<PatFile::Handle>(i);
	}

	return PatFile::kInvalid;
}

}

PatFile::Handle PatFile::LoadFromMemory(const char* name, const uint8_t* data, size_t size)
{
	if (name == nullptr || data == nullptr || size <= kBody)
		return kInvalid;

	const Handle known = Find(name);

	if (known != kInvalid)
		return known;

	Document* doc = new Document();
	doc->path = name;
	doc->blob.assign(data, data + size);

	return Adopt(doc);
}

PatFile::Handle PatFile::Load(const char* path)
{
	if (path == nullptr || path[0] == 0)
		return kInvalid;

	const Handle known = Find(path);

	if (known != kInvalid)
		return known;

	FILE* file = nullptr;

	if (fopen_s(&file, path, "rb") != 0 || file == nullptr)
	{
		sprintf_s(g_status, "cannot open %s", path);
		return kInvalid;
	}

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size <= static_cast<long>(kBody))
	{
		fclose(file);
		sprintf_s(g_status, "%s is too small to be a .pat", path);
		return kInvalid;
	}

	Document* doc = new Document();
	doc->path = path;
	doc->blob.resize(static_cast<size_t>(size));

	const size_t read = fread(doc->blob.data(), 1, doc->blob.size(), file);
	fclose(file);

	if (read != doc->blob.size())
	{
		delete doc;
		sprintf_s(g_status, "read of %s stopped short", path);
		return kInvalid;
	}

	return Adopt(doc);
}

void PatFile::Unload(Handle handle)
{
	Document* doc = Get(handle);

	if (doc == nullptr)
		return;

	delete doc;
	g_documents[handle] = nullptr;
}

void PatFile::UnloadAll()
{
	for (Document* doc : g_documents)
		delete doc;

	g_documents.clear();
}

const char* PatFile::Path(Handle handle)
{
	const Document* doc = Get(handle);

	return doc != nullptr ? doc->path.c_str() : "";
}

int PatFile::AtlasCount(Handle handle)
{
	const Document* doc = Get(handle);

	return doc != nullptr ? static_cast<int>(doc->atlases.size()) : 0;
}

bool PatFile::GetAtlas(Handle handle, int index, Atlas& out)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || index < 0 || index >= static_cast<int>(doc->atlases.size()))
		return false;

	out = doc->atlases[index];
	return true;
}

int PatFile::PartCount(Handle handle)
{
	const Document* doc = Get(handle);

	return doc != nullptr ? static_cast<int>(doc->parts.size()) : 0;
}

bool PatFile::GetPart(Handle handle, int id, Part& out)
{
	const Document* doc = Get(handle);

	if (doc == nullptr)
		return false;

	const auto found = doc->parts.find(id);

	if (found == doc->parts.end())
		return false;

	out = found->second.part;
	out.name = found->second.name.c_str();
	return true;
}

int PatFile::FindPartBySuffix(Handle handle, const char* suffix)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || suffix == nullptr || *suffix == 0)
		return -1;

	const size_t tail = strlen(suffix);

	int best = -1;
	int bestArea = 0;

	for (const auto& entry : doc->parts)
	{
		const std::string& name = entry.second.name;

		if (name.size() < tail || _stricmp(name.c_str() + name.size() - tail, suffix) != 0)
			continue;

		const int area = entry.second.part.width * entry.second.part.height;

		if (area <= bestArea)
			continue;

		bestArea = area;
		best = entry.first;
	}

	return best;
}

bool PatFile::AtlasSize(Handle handle, int atlas, int& width, int& height)
{
	const Document* doc = Get(handle);

	if (doc == nullptr)
		return false;

	for (const Atlas& entry : doc->atlases)
	{
		if (entry.id != atlas)
			continue;

		width = entry.width;
		height = entry.height;
		return true;
	}

	return false;
}

int PatFile::PatternCount(Handle handle)
{
	const Document* doc = Get(handle);

	return doc != nullptr ? static_cast<int>(doc->patterns.size()) : 0;
}

const char* PatFile::PatternName(Handle handle, int index)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || index < 0 || index >= static_cast<int>(doc->patterns.size()))
		return nullptr;

	return doc->patterns[index].name.c_str();
}

int PatFile::FindPattern(Handle handle, const char* name)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || name == nullptr)
		return -1;

	for (size_t i = 0; i < doc->patterns.size(); ++i)
	{
		if (doc->patterns[i].name == name)
			return static_cast<int>(i);
	}

	return -1;
}

int PatFile::SpriteCount(Handle handle, int pattern)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || pattern < 0 || pattern >= static_cast<int>(doc->patterns.size()))
		return 0;

	return static_cast<int>(doc->patterns[pattern].sprites.size());
}

const PatFile::Sprite* PatFile::GetSprites(Handle handle, int pattern)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || pattern < 0 || pattern >= static_cast<int>(doc->patterns.size()))
		return nullptr;

	return doc->patterns[pattern].sprites.data();
}

const char* PatFile::StatusText()
{
	return g_status;
}
