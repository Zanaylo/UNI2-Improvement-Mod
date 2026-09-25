#pragma once

#include <string>
#include <vector>

namespace UserMusic
{
	enum Status
	{
		Status_Ready,
		Status_Converting,
		Status_Rejected,
	};

	struct Entry
	{
		std::string pack;
		std::string fileName;
		std::string title;
		std::string slotName;
		std::string sourcePath;
		std::string playPath;
		Status status;
		std::string note;
		double loopPos;
	};

	void Scan();

	int Version();
	std::vector<Entry> Snapshot();

	bool ConsumeChanged();

	bool Import(const std::string& file, const std::string& pack, char* status, int statusSize);

	void SetLoopPoint(const std::string& slotName, double seconds);

	std::string Root();
	std::string CacheRoot();
	std::string PackFolder(const std::string& pack);

	const char* SupportedFilter();
}
