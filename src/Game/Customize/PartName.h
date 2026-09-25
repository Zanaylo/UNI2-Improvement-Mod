#pragma once

namespace PartName
{
	constexpr int kMaxLength = 32;

	void Get(int chara, int entry, char* out, int size);
	void Set(int chara, int entry, const char* name);
}
