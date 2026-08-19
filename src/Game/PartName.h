// What the user calls a character's effect entry. The game names its parts in Japanese and this
// mod is English only, so the name is theirs to type and it is kept per character in the ini.

#pragma once

namespace PartName
{
	constexpr int kMaxLength = 32;

	void Get(int chara, int entry, char* out, int size);
	void Set(int chara, int entry, const char* name);
}
