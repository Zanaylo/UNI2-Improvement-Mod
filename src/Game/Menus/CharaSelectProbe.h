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
