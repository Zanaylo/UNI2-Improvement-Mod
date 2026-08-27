#pragma once

#include <cstdint>

namespace ModPresence
{
	constexpr int kMaxMembers = 16;

	void Update();

	bool InRoom();

	int RoomSize();
	int ModCount();

	bool HasMod(int index);
	uint64_t MemberAt(int index);
	const char* VersionAt(int index);

	const char* GetStatusText();
}
