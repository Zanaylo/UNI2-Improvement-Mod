#pragma once

#include <cstdint>

namespace RoomRoster
{
	constexpr int kMaxEvents = 64;

	enum StateChange
	{
		StateChange_Entered = 1,
		StateChange_Left = 2,
		StateChange_Disconnected = 4,
		StateChange_Kicked = 8,
		StateChange_Banned = 16
	};

	struct Event
	{
		unsigned tick;
		uint64_t user;
		int rawFlags;
		bool rewritten;
	};

	bool Initialize();
	bool IsHooked();

	bool IsFixEnabled();
	void SetFixEnabled(bool enabled);

	int EventCount();
	const Event& GetEvent(int index);

	int GetGhostsPrevented();

	const char* DescribeFlags(int flags, char* out, int size);

	void WriteCrashReport();

	const char* GetStatusText();
}
