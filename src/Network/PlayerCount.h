#pragma once

namespace PlayerCount
{
	void Update();

	bool IsKnown();
	int Get();

	const char* GetStatusText();
}
