#pragma once

#include <cstdint>

namespace PaletteShare
{
	void Initialize();

	void OnFrame();

	const char* GetStatusText();

	int GetOwnSide();

	const char* GetRemoteName(int player);

	struct Diagnostics
	{
		int ownSide;
		int framesInMatch;
		int sendsDone;
		int sendsAllowed;
		int nextSendFrame;
		int sent;
		int received;
		uint64_t matchPeer;
		int steamAttempts;
		int lastSentChoice;
		int resends;

		bool pendingValid[2];
		bool pendingApplied[2];
		int pendingChara[2];
		const char* pendingName[2];
	};

	void GetDiagnostics(Diagnostics& out);

	void InjectTestPacket(int side);
}
