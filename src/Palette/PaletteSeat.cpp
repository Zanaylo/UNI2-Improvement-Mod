#include "Palette/PaletteSeat.h"

#include "Training/FrameStepper.h"
#include "Core/utils.h"

#include <cstring>

namespace {

constexpr int kStaleFrames = 120;
constexpr int kLiveFrames = 4;

PaletteSeat::Seat g_seats[PaletteSeat::kMaxSeats] = {};
int g_count = 0;
int g_frame = 0;

struct Candidate
{
	uintptr_t texture;
	int draws;
	int lastSeenFrame;
};

Candidate g_candidates[PaletteSeat::kMaxSeats][PaletteSeat::kCandidates] = {};

bool IsLive(const PaletteSeat::Seat& seat)
{
	return seat.texture != 0 && g_frame - seat.lastSeenFrame <= kLiveFrames;
}

const PaletteSeat::Seat* Busiest(int side, bool liveOnly)
{
	const PaletteSeat::Seat* best = nullptr;

	for (int i = 0; i < g_count; ++i)
	{
		const PaletteSeat::Seat& seat = g_seats[i];

		if (seat.side != side || seat.texture == 0)
			continue;

		if (liveOnly && !IsLive(seat))
			continue;

		if (best == nullptr || seat.draws > best->draws)
			best = &seat;
	}

	return best;
}

const PaletteSeat::Seat* Occupant(int side)
{
	if (side < 0 || side >= PaletteSeat::kSides)
		return nullptr;

	const PaletteSeat::Seat* const live = Busiest(side, true);

	return live != nullptr ? live : Busiest(side, false);
}

int FindSeat(uintptr_t owner)
{
	for (int i = 0; i < g_count; ++i)
	{
		if (g_seats[i].owner == owner)
			return i;
	}

	return -1;
}

bool IsLiveCandidate(const Candidate& candidate)
{
	return candidate.texture != 0 && g_frame - candidate.lastSeenFrame <= kLiveFrames;
}

void Take(Candidate& candidate, uintptr_t texture)
{
	candidate.texture = texture;
	candidate.draws = 1;
	candidate.lastSeenFrame = g_frame;
}

void Remember(int seat, uintptr_t texture)
{
	Candidate* const row = g_candidates[seat];

	for (int i = 0; i < PaletteSeat::kCandidates; ++i)
	{
		if (row[i].texture != texture)
			continue;

		++row[i].draws;
		row[i].lastSeenFrame = g_frame;
		return;
	}

	for (int i = 0; i < PaletteSeat::kCandidates; ++i)
	{
		if (IsLiveCandidate(row[i]))
			continue;

		Take(row[i], texture);
		return;
	}

	int weakest = 0;

	for (int i = 1; i < PaletteSeat::kCandidates; ++i)
	{
		if (row[i].draws < row[weakest].draws)
			weakest = i;
	}

	if (--row[weakest].draws <= 0)
		Take(row[weakest], texture);
}

bool Beats(const Candidate& candidate, const Candidate& other)
{
	const bool live = IsLiveCandidate(candidate);

	if (live != IsLiveCandidate(other))
		return live;

	return candidate.draws > other.draws;
}

int BestCandidate(int seat, const bool* taken)
{
	const Candidate* const row = g_candidates[seat];

	int best = -1;

	for (int i = 0; i < PaletteSeat::kCandidates; ++i)
	{
		if (row[i].texture == 0 || (taken != nullptr && taken[i]))
			continue;

		if (best < 0 || Beats(row[i], row[best]))
			best = i;
	}

	return best;
}

uintptr_t ChosenCandidate(int seat)
{
	const int best = BestCandidate(seat, nullptr);

	return best >= 0 ? g_candidates[seat][best].texture : 0;
}

void Drop(int index)
{
	for (int i = index; i < g_count - 1; ++i)
	{
		g_seats[i] = g_seats[i + 1];
		memcpy(g_candidates[i], g_candidates[i + 1], sizeof(g_candidates[i]));
	}

	--g_count;

	memset(&g_seats[g_count], 0, sizeof(g_seats[g_count]));
	memset(g_candidates[g_count], 0, sizeof(g_candidates[g_count]));
}

}

void PaletteSeat::OnDraw(uintptr_t owner, uintptr_t texture, int side)
{
	if (owner == 0 || texture == 0 || side < 0)
		return;

	int seat = FindSeat(owner);

	if (seat < 0)
	{
		if (g_count >= kMaxSeats)
			return;

		seat = g_count++;

		memset(&g_seats[seat], 0, sizeof(g_seats[seat]));
		memset(g_candidates[seat], 0, sizeof(g_candidates[seat]));

		g_seats[seat].owner = owner;
	}

	Seat& entry = g_seats[seat];

	entry.side = side;

	if (side < 32)
		entry.rows |= 1u << side;

	entry.lastSeenFrame = g_frame;
	++entry.draws;

	Remember(seat, texture);
	entry.texture = ChosenCandidate(seat);
}

void PaletteSeat::OnFrame()
{
	// A seat ages by presented frames, and it is a draw call that keeps it alive. Tick stop does not
	// run the game's frame at all, so nothing draws and every seat would age out in two seconds of
	// being paused - taking the texture identity, the paint and the effect colours with it. The
	// absence of draws here is the mod's own doing, not evidence the character has gone.
	if (FrameStepper::IsFrozen())
		return;

	++g_frame;

	for (int i = g_count - 1; i >= 0; --i)
	{
		if (g_frame - g_seats[i].lastSeenFrame > kStaleFrames)
			Drop(i);
	}
}

int PaletteSeat::GetSeatCount()
{
	return g_count;
}

bool PaletteSeat::GetSeat(int index, Seat& out)
{
	if (index < 0 || index >= g_count)
		return false;

	out = g_seats[index];
	return true;
}

uintptr_t PaletteSeat::GetTexture(int side)
{
	const Seat* const seat = Occupant(side);

	return seat != nullptr ? seat->texture : 0;
}

uint32_t PaletteSeat::GetRows(int side)
{
	const Seat* const seat = Occupant(side);

	return seat != nullptr ? seat->rows : 0;
}

uintptr_t PaletteSeat::GetOwner(int side)
{
	const Seat* const seat = Occupant(side);

	return seat != nullptr ? seat->owner : 0;
}

bool PaletteSeat::GetByOwner(uintptr_t owner, uintptr_t& outTexture, uint32_t& outRows)
{
	for (int i = 0; i < g_count; ++i)
	{
		if (g_seats[i].owner != owner || !IsLive(g_seats[i]))
			continue;

		outTexture = g_seats[i].texture;
		outRows = g_seats[i].rows;
		return true;
	}

	return false;
}

int PaletteSeat::GetCandidates(uintptr_t owner, uintptr_t* out, int max)
{
	if (owner == 0 || out == nullptr || max <= 0)
		return 0;

	const int seat = FindSeat(owner);

	if (seat < 0 || !IsLive(g_seats[seat]))
		return 0;

	bool taken[kCandidates] = {};
	int count = 0;

	while (count < max)
	{
		const int best = BestCandidate(seat, taken);

		if (best < 0)
			break;

		taken[best] = true;
		out[count++] = g_candidates[seat][best].texture;
	}

	return count;
}

int PaletteSeat::GetSideByOwner(uintptr_t owner)
{
	if (owner == 0)
		return -1;

	for (int i = 0; i < g_count; ++i)
	{
		if (g_seats[i].owner == owner)
			return g_seats[i].side;
	}

	return -1;
}

int PaletteSeat::GetFrame()
{
	return g_frame;
}

void PaletteSeat::Reset()
{
	g_count = 0;
	memset(g_seats, 0, sizeof(g_seats));
	memset(g_candidates, 0, sizeof(g_candidates));
}
