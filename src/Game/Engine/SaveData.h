#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SaveData
{
	enum class Mode
	{
		Idle = 0,
		Load = 1,
		Save = 2,
	};

	struct State
	{
		bool dirty;
		bool enabled;
		bool requested;
		bool buffered;
		int mode;
		uint32_t machine;
		uint32_t size;
		bool headerValid;
	};

	bool Read(State& out);

	bool Request();
	bool MarkDirty();

	bool IsPending();
	bool IsBusy();
	bool Start();
	bool Pump();

	struct File
	{
		std::string path;
		uint64_t size;
		uint64_t written;
	};

	bool ListFiles(std::vector<File>& out);
}
