// Which field of the character-select state holds the highlighted character.
//
// `CharaSelectState` reaches the block; this watches every dword in it while the screen is up and
// reports the ones that settle on a plausible character and then move, which is what a cursor does
// and what a frame counter never does. It reports rather than decides: the offset it names goes
// into screen.ini as StateField, so nothing here has to be rebuilt to act on the answer.

#pragma once

#include <cstdint>

namespace CharaSelectProbe
{
	constexpr int kMaxCandidates = 12;

	struct Candidate
	{
		uintptr_t rva;
		uint32_t value;
		int changes;
	};

	void OnFrame();
	void Summarise();

	int CandidateCount();
	bool GetCandidate(int index, Candidate& out);

	const char* StatusText();
}
