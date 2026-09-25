#pragma once

namespace GrdWatch
{
	constexpr int kPlayers = 2;
	constexpr int kMaxPopups = 16;
	constexpr int kUnitsPerBlock = 10000;
	constexpr int kLifeFrames = 60;

	struct Popup
	{
		int side;
		int amount;
		int age;
	};

	bool IsAllowedHere();
	bool IsAllowed();

	void SampleFromGameThread();
	void Reset();

	int GetPopups(Popup* out, int max);

	bool ReadGrd(int player, int& outSide, int& outUnits);

	bool ReadCycleRemaining(int& outFrames);
}
