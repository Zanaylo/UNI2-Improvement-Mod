#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <Windows.h>

namespace ReplayFiles
{
	constexpr size_t kRecordSize = 0x7a88;
	constexpr int kSlotCount = 400;

	struct Info
	{
		bool used;
		bool locked;
		int version;
		SYSTEMTIME time;
		int chara[2];
		uint64_t steamId[2];
		std::string title[2];
		std::string name[2];
	};

	bool IsAvailable();

	bool IsLive();

	bool ReadInfo(int slot, Info& out);
	int CountUsed();

	std::string PlayerName(const Info& info, int player);

	std::string DescribeSlot(const Info& info);
	std::string FileNameFor(const Info& info, int matchNumber);

	std::string Export(int slot);
	int ExportAll(std::string& outError);

	bool Import(const std::string& path, std::string& outError);

	bool RequestPlayback(const std::string& path, std::string& outError);

	bool ReadRecordFile(const std::string& path, std::vector<uint8_t>& out, std::string outNames[2],
		std::string& outError);
	bool CanPlay();

	bool Initialize();

	void OnGameFrame();

	bool IsPlaybackSession();

	void SetLeaveDeadList(bool leave);
	bool GetLeaveDeadList();

	void SetHoldNativePads(bool hold);
	bool GetHoldNativePads();
	void HoldNativePadsNow();

	int FindFreeSlot();

	int CurrentVersion();

	void SetAutoExport(bool enabled);
	bool GetAutoExport();

	void Update();

	const char* GetStatus();

	void ReadInputBlock(int& outCount, int& outLevel);
	bool ClearInputBlock();

	const char* DescribeInputState();

	std::string GetFolder();
	const std::vector<std::string>& ListFiles();

	void Refresh();
}
