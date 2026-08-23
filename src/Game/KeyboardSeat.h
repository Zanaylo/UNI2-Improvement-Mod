// Which side the keyboard plays in an offline match: the keyboard keeps the configuration it
// already has, and the pad is what moves.
//
// A side reads its input from the pad slot `[0x5def24 + side*4]`, and that slot is both the pad port
// and the keyboard player number. The first pad is device 0 and therefore port 0, the keyboard's
// configured keys are keyboard player 0 and therefore also port 0, so a keyboard and a single
// controller land on the same side. That is the whole of the problem.
//
// Two things this may not do, both learned the hard way:
//
// - **It may not write the key maps.** Everything from `0x59a520` for `0x136e1` bytes is mirrored
//   into `SYS-DATA` byte for byte, so a write there is a write to the player's save file.
// - **It may not move the keyboard between players.** Which keyboard player a reader asks for is not
//   one question: `0x244980` asks for the side's pad slot, and the Restart hotkey at `0x15a110` asks
//   for the side itself. Move the keyboard and one of the two is always left reading the wrong run -
//   which is exactly how Restart stopped working.
//
// So the keyboard is never touched. The **pad** moves: the two raw port records are exchanged around
// the pad update, which puts the controller on port 1 and leaves port 0 to the keyboard alone, and
// the two sides' slots are written so the chosen side holds port 0. `[0x753e0c]` is raised so port 1
// counts as a port at all. All three are runtime state, in no file, saved and put back on release -
// and the slots especially, because a slot left pointing at a port that is no longer valid is what
// puts "Please connect wireless controller 2" on the screen.

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
