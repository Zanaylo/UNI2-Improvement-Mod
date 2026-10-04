#pragma once

#include "Screens/PatReader.h"

#include <cstddef>
#include <cstdint>

namespace PatFile
{
	using Handle = int;

	constexpr Handle kInvalid = -1;

	using Blend = PatReader::Blend;
	using Atlas = PatReader::Atlas;
	using Part = PatReader::Part;
	using Sprite = PatReader::Sprite;

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
