#pragma once

namespace OnlineSafety
{
	bool IsGuarded();
	void SetGuarded(bool guarded);

	bool InSession();

	bool MayWriteRoomState();

	bool MayCallSession();

	void Update();

	const char* GetStatusText();
}
