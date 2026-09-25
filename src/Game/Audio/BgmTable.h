#pragma once

namespace BgmTable
{
	constexpr int kSlotCount = 200;

	struct Entry
	{
		bool present;
		bool loops;
		double loopPos;
		int volume;
		int noRecording;
		char file[33];
	};

	bool Read(int id, Entry& out);

	bool Bind(int id, const Entry& entry);

	bool GetVolume(int id, int& out);
	bool SetVolume(int id, int volume);

	bool IsPresent(int id);

	int CollectPresent(int* outIds, int maxIds);

	const char* DescribeSlot(int id);
}
