#include "Game/CharaSelectProbe.h"

#include "Core/logger.h"
#include "Game/CharaSelectState.h"
#include "Game/GameOffsets.h"
#include "Game/SceneWatch.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int kSides = CharaSelectState::kSideCount;
constexpr int kDwords = 0xc2c / 4;
constexpr int kWatched = kSides * kDwords;

constexpr int kMaxLogged = 120;
constexpr int kTrail = 10;
constexpr uint32_t kPlausible = 32;
constexpr int kSteadyFrames = 10;

uint32_t g_scratch[kDwords] = {};
uint32_t g_previous[kWatched] = {};
uint32_t g_trail[kWatched][kTrail] = {};
int g_trailCount[kWatched] = {};
int g_changes[kWatched] = {};
int g_held[kWatched] = {};
bool g_primed = false;
bool g_summarised = false;
int g_logged = 0;
char g_status[96] = "not on character select";

int IndexOf(int side, int dword)
{
	return side * kDwords + dword;
}

bool Interesting(int index)
{
	return g_changes[index] > 0 && g_changes[index] <= 40 && g_trailCount[index] > 1;
}

}

void CharaSelectProbe::Summarise()
{
	if (g_summarised)
		return;

	g_summarised = true;

	for (int side = 0; side < kSides; ++side)
	{
		for (int dword = 0; dword < kDwords; ++dword)
		{
			const int index = IndexOf(side, dword);

			if (!Interesting(index))
				continue;

			char trail[160] = {};
			int at = 0;

			for (int step = 0; step < g_trailCount[index] && at < 140; ++step)
				at += sprintf_s(trail + at, sizeof(trail) - at, "%s%u", step > 0 ? "," : "",
					g_trail[index][step]);

			LOG("CharaSelectProbe: %dP +0x%03x settled %d time(s): %s", side + 1, dword * 4,
				g_changes[index], trail);
		}
	}
}

void CharaSelectProbe::OnFrame()
{
	if (SceneWatch::Current() != GameOffsets::kSceneCharaSelect)
	{
		if (g_primed)
			Summarise();

		g_primed = false;
		return;
	}

	int moved = 0;

	for (int side = 0; side < kSides; ++side)
	{
		if (!CharaSelectState::ReadBlock(side, g_scratch, kDwords))
		{
			strncpy_s(g_status, "the state block is unreadable", _TRUNCATE);
			return;
		}

		for (int dword = 0; dword < kDwords; ++dword)
		{
			const int index = IndexOf(side, dword);
			const uint32_t value = g_scratch[dword];

			if (!g_primed)
			{
				g_previous[index] = value;
				continue;
			}

			if (value == g_previous[index])
			{
				++g_held[index];
				continue;
			}

			const uint32_t was = g_previous[index];

			if (g_held[index] >= kSteadyFrames && was <= kPlausible && value <= kPlausible)
			{
				++g_changes[index];

				if (g_trailCount[index] < kTrail)
					g_trail[index][g_trailCount[index]++] = was;

				if (g_logged < kMaxLogged)
				{
					++g_logged;
					LOG("CharaSelectProbe: %dP +0x%03x %u -> %u after %d frame(s)", side + 1,
						dword * 4, was, value, g_held[index]);
				}
			}

			g_previous[index] = value;
			g_held[index] = 0;
			++moved;
		}
	}

	if (!g_primed)
	{
		g_primed = true;
		g_summarised = false;
		return;
	}

	if (moved > 0)
		sprintf_s(g_status, "%d dword(s) moved this frame", moved);
}

int CharaSelectProbe::CandidateCount()
{
	int found = 0;

	for (int i = 0; i < kWatched && found < kMaxCandidates; ++i)
	{
		if (Interesting(i))
			++found;
	}

	return found;
}

bool CharaSelectProbe::GetCandidate(int index, Candidate& out)
{
	int found = 0;

	for (int i = 0; i < kWatched; ++i)
	{
		if (!Interesting(i))
			continue;

		if (found++ != index)
			continue;

		out.rva = static_cast<uintptr_t>((i % kDwords) * 4);
		out.value = g_previous[i];
		out.changes = g_changes[i];
		return true;
	}

	return false;
}

const char* CharaSelectProbe::StatusText()
{
	return g_status;
}
