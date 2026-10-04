#include "Screens/PatFile.h"

#include "Core/logger.h"
#include "Screens/PatReader.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr size_t kBody = 0x20;

struct Document
{
	std::string path;
	std::vector<uint8_t> blob;
	PatReader::Document content;
};

std::vector<Document*> g_documents;
char g_status[192] = "nothing loaded";

PatFile::Handle Adopt(Document* doc)
{
	PatReader::Read(doc->blob, doc->content);
	g_documents.push_back(doc);

	sprintf_s(g_status, "%d atlas(es), %d part(s), %d pattern(s)",
		static_cast<int>(doc->content.atlases.size()), static_cast<int>(doc->content.parts.size()),
		static_cast<int>(doc->content.patterns.size()));

	LOG("PatFile: %s - %s", doc->path.c_str(), g_status);

	return static_cast<PatFile::Handle>(g_documents.size() - 1);
}

Document* Get(PatFile::Handle handle)
{
	if (handle < 0 || handle >= static_cast<int>(g_documents.size()))
		return nullptr;

	return g_documents[handle];
}

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

	return doc != nullptr ? static_cast<int>(doc->content.atlases.size()) : 0;
}

bool PatFile::GetAtlas(Handle handle, int index, Atlas& out)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || index < 0 || index >= static_cast<int>(doc->content.atlases.size()))
		return false;

	out = doc->content.atlases[index];
	return true;
}

int PatFile::PartCount(Handle handle)
{
	const Document* doc = Get(handle);

	return doc != nullptr ? static_cast<int>(doc->content.parts.size()) : 0;
}

bool PatFile::GetPart(Handle handle, int id, Part& out)
{
	const Document* doc = Get(handle);

	return doc != nullptr && PatReader::PartOf(doc->content, id, out);
}

int PatFile::FindPartBySuffix(Handle handle, const char* suffix)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || suffix == nullptr || *suffix == 0)
		return -1;

	const size_t tail = strlen(suffix);

	int best = -1;
	int bestArea = 0;

	for (const auto& entry : doc->content.parts)
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

	const Atlas* const found = doc != nullptr ? PatReader::AtlasOf(doc->content, atlas) : nullptr;

	if (found == nullptr)
		return false;

	width = found->width;
	height = found->height;

	return true;
}

int PatFile::PatternCount(Handle handle)
{
	const Document* doc = Get(handle);

	return doc != nullptr ? static_cast<int>(doc->content.patterns.size()) : 0;
}

const char* PatFile::PatternName(Handle handle, int index)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || index < 0 || index >= static_cast<int>(doc->content.patterns.size()))
		return nullptr;

	return doc->content.patterns[index].name.c_str();
}

int PatFile::FindPattern(Handle handle, const char* name)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || name == nullptr)
		return -1;

	for (size_t i = 0; i < doc->content.patterns.size(); ++i)
	{
		if (doc->content.patterns[i].name == name)
			return static_cast<int>(i);
	}

	return -1;
}

int PatFile::SpriteCount(Handle handle, int pattern)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || pattern < 0 || pattern >= static_cast<int>(doc->content.patterns.size()))
		return 0;

	return static_cast<int>(doc->content.patterns[pattern].sprites.size());
}

const PatFile::Sprite* PatFile::GetSprites(Handle handle, int pattern)
{
	const Document* doc = Get(handle);

	if (doc == nullptr || pattern < 0 || pattern >= static_cast<int>(doc->content.patterns.size()))
		return nullptr;

	return doc->content.patterns[pattern].sprites.data();
}

const char* PatFile::StatusText()
{
	return g_status;
}
