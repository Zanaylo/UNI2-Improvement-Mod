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
