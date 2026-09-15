#pragma once

#include <cstdint>
#include <vector>

namespace SpectateHost
{
	constexpr int kMostViewers = 24;

	enum ViewerState
	{
		Viewer_Pending,
		Viewer_Accepted,
		Viewer_Kicked
	};

	struct Viewer
	{
		uint64_t id;
		ViewerState state;
		uint32_t armedAt;
		bool prepared;
		bool watching;
	};

	void Initialize();
	void Update();

	void Receive(uint8_t type, uint64_t from);

	bool IsAllowed();
	void SetAllowed(bool allowed);

	int MaxViewers();
	void SetMaxViewers(int count);

	const char* Code();

	void Snapshot(std::vector<Viewer>& out);

	void Approve(uint64_t id);
	void Kick(uint64_t id);
	void Forget(uint64_t id);

	const char* StatusText();
}
