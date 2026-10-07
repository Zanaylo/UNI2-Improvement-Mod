#include "Palette/IconEntryVote.h"

#include <cstring>

namespace {

constexpr int kRgb = 3;
constexpr int kRgbaStride = 4;

int FindEntries(const uint8_t* palette, const uint8_t* icon, int* out)
{
	int found = 0;

	for (int entry = 0; entry < IconEntryVote::kEntries; ++entry)
	{
		if (memcmp(palette + entry * kRgbaStride, icon, kRgb) == 0)
			out[found++] = entry;
	}

	return found;
}

void CastVote(const uint8_t* palette, const uint8_t* icon, double* votes)
{
	int entries[IconEntryVote::kEntries] = {};
	const int found = FindEntries(palette, icon, entries);

	for (int i = 0; i < found; ++i)
		votes[entries[i]] += 1.0 / found;
}

void TallyVotes(const uint8_t* const* palettes, const uint8_t* const* icons, int count, const int* taken,
	int takenCount, double* votes)
{
	for (int colour = 0; colour < count; ++colour)
	{
		if (palettes[colour] == nullptr || icons[colour] == nullptr)
			continue;

		CastVote(palettes[colour], icons[colour], votes);
	}

	for (int i = 0; i < takenCount; ++i)
	{
		if (taken[i] >= 0 && taken[i] < IconEntryVote::kEntries)
			votes[taken[i]] = 0.0;
	}
}

bool Beats(int entry, int winner, const double* votes)
{
	return votes[entry] > 0.0 && (winner == IconEntryVote::kNone || votes[entry] > votes[winner]);
}

int MostVoted(const double* votes)
{
	int winner = IconEntryVote::kNone;

	for (int entry = 0; entry < IconEntryVote::kEntries; ++entry)
	{
		if (Beats(entry, winner, votes))
			winner = entry;
	}

	return winner;
}

int MostVotedOf(const int* candidates, int count, const double* votes)
{
	int winner = IconEntryVote::kNone;

	for (int i = 0; i < count; ++i)
	{
		if (Beats(candidates[i], winner, votes))
			winner = candidates[i];
	}

	return winner;
}

}

int IconEntryVote::Winner(const uint8_t* const* palettes, const uint8_t* const* icons, int count,
	const int* taken, int takenCount)
{
	double votes[kEntries] = {};
	TallyVotes(palettes, icons, count, taken, takenCount, votes);
	return MostVoted(votes);
}

int IconEntryVote::FirstColourWinner(const uint8_t* const* palettes, const uint8_t* const* icons, int count,
	const int* taken, int takenCount)
{
	double votes[kEntries] = {};
	TallyVotes(palettes, icons, count, taken, takenCount, votes);

	if (count <= 0 || palettes[0] == nullptr || icons[0] == nullptr)
		return MostVoted(votes);

	int candidates[kEntries] = {};
	const int found = FindEntries(palettes[0], icons[0], candidates);
	const int winner = MostVotedOf(candidates, found, votes);

	return winner != kNone ? winner : MostVoted(votes);
}
