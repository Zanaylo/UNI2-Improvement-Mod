#pragma once

namespace KeyboardSeat
{
	enum Seat
	{
		Seat_Default = 0,
		Seat_P1 = 1,
		Seat_P2 = 2
	};

	bool Initialize();

	void ApplySaved();

	void SetSeat(int seat);
	int GetSeat();

	void SetRouteSides(bool route);
	bool GetRouteSides();

	void OnFrameUpdate();
	void Update();

	bool IsAvailable();
	const char* GetStatus();

	const char* GetSeatName(int seat);
}
