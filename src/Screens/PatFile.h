// A reader for a whole .pat: its atlases, the parts cut out of them, and the patterns that say
// where each part is drawn. `Game/PatParts` is the other reader and a different job - it builds the
// colour editor's 3D effect meshes and hands back CPU pixels. This one is for drawing a screen.
//
// The container has no chunk lengths, so an unknown tag is passed by scanning for the next known
// one, never by a fixed size. An atlas comes back as the DDS it is stored as, decompressed if it
// was packed, because `DdsTexture::LoadFromMemory` already takes both formats these games use.

#pragma once

#include <cstddef>
#include <cstdint>

namespace PatFile
{
	using Handle = int;

	constexpr Handle kInvalid = -1;

	enum Blend
	{
		Blend_Normal = 0,
		Blend_Additive = 1,
		Blend_Subtractive = 2,
	};

	struct Atlas
	{
		int id;
		int width;
		int height;
		const uint8_t* dds;
		size_t ddsSize;
	};

	struct Part
	{
		int id;
		int atlas;
		int u;
		int v;
		int w;
		int h;
		int width;
		int height;
		int pivotX;
		int pivotY;
		const char* name;
	};

	struct Sprite
	{
		int part;
		int x;
		int y;
		float zoomX;
		float zoomY;
		uint32_t tint;
		int priority;
		int blend;
		float turns;
	};

	Handle Load(const char* path);
	Handle LoadFromMemory(const char* name, const uint8_t* data, size_t size);
	void Unload(Handle handle);
	void UnloadAll();

	const char* Path(Handle handle);

	int AtlasCount(Handle handle);
	bool GetAtlas(Handle handle, int index, Atlas& out);

	int PartCount(Handle handle);
	bool GetPart(Handle handle, int id, Part& out);
	int FindPartBySuffix(Handle handle, const char* suffix);
	bool AtlasSize(Handle handle, int atlas, int& width, int& height);

	int PatternCount(Handle handle);
	const char* PatternName(Handle handle, int index);
	int FindPattern(Handle handle, const char* name);

	int SpriteCount(Handle handle, int pattern);
	const Sprite* GetSprites(Handle handle, int pattern);

	const char* StatusText();
}
