#include "Screens/PatTextures.h"

#include "Core/logger.h"
#include "D3D9/DdsTexture.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using AtlasTextures = std::vector<std::pair<int, IDirect3DTexture9*>>;

std::unordered_map<PatFile::Handle, AtlasTextures> g_textures;
std::unordered_map<PatFile::Handle, bool> g_prepared;

IDirect3DTexture9* FindAtlasTexture(const AtlasTextures& atlases, int atlas)
{
	for (const auto& entry : atlases)
	{
		if (entry.first == atlas)
			return entry.second;
	}

	return nullptr;
}

}

bool PatTextures::Prepare(IDirect3DDevice9* device, PatFile::Handle handle)
{
	if (device == nullptr || handle == PatFile::kInvalid)
		return false;

	const auto done = g_prepared.find(handle);

	if (done != g_prepared.end())
		return done->second;

	const int count = PatFile::AtlasCount(handle);
	int uploaded = 0;

	AtlasTextures& atlases = g_textures[handle];

	for (int i = 0; i < count; ++i)
	{
		PatFile::Atlas atlas = {};

		if (!PatFile::GetAtlas(handle, i, atlas))
			continue;

		unsigned width = 0;
		unsigned height = 0;
		IDirect3DTexture9* texture = DdsTexture::LoadFromMemory(device, atlas.dds, atlas.ddsSize,
			PatFile::Path(handle), width, height);

		if (texture == nullptr)
			continue;

		atlases.emplace_back(atlas.id, texture);
		++uploaded;
	}

	g_prepared[handle] = uploaded > 0;

	LOG("PatTextures: %d of %d atlas(es) of %s uploaded", uploaded, count, PatFile::Path(handle));

	return uploaded > 0;
}

IDirect3DTexture9* PatTextures::Get(PatFile::Handle handle, int atlas)
{
	const auto found = g_textures.find(handle);

	return found != g_textures.end() ? FindAtlasTexture(found->second, atlas) : nullptr;
}

void PatTextures::Release()
{
	for (auto& handleEntry : g_textures)
	{
		for (auto& atlasEntry : handleEntry.second)
		{
			if (atlasEntry.second != nullptr)
				atlasEntry.second->Release();
		}
	}

	g_textures.clear();
	g_prepared.clear();
}

int PatTextures::Count()
{
	int count = 0;

	for (const auto& handleEntry : g_textures)
		count += static_cast<int>(handleEntry.second.size());

	return count;
}
