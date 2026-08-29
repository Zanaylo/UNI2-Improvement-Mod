#include "Screens/PatTextures.h"

#include "Core/logger.h"
#include "D3D9/DdsTexture.h"

#include <map>
#include <utility>

namespace {

using Key = std::pair<PatFile::Handle, int>;

std::map<Key, IDirect3DTexture9*> g_textures;
std::map<PatFile::Handle, bool> g_prepared;

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

		g_textures[Key(handle, atlas.id)] = texture;
		++uploaded;
	}

	g_prepared[handle] = uploaded > 0;

	LOG("PatTextures: %d of %d atlas(es) of %s uploaded", uploaded, count, PatFile::Path(handle));

	return uploaded > 0;
}

IDirect3DTexture9* PatTextures::Get(PatFile::Handle handle, int atlas)
{
	const auto found = g_textures.find(Key(handle, atlas));

	return found != g_textures.end() ? found->second : nullptr;
}

void PatTextures::OnDeviceLost()
{
	for (auto& entry : g_textures)
	{
		if (entry.second != nullptr)
			entry.second->Release();
	}

	g_textures.clear();
	g_prepared.clear();
}

int PatTextures::Count()
{
	return static_cast<int>(g_textures.size());
}
