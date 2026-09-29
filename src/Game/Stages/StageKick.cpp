#include "Game/Stages/StageKick.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/Camera.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/MemoryMap.h"
#include "Game/Stages/StagePlacement.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr size_t kStackBegin = 0x160;
constexpr size_t kStackEnd = 0x164;
constexpr ptrdiff_t kMatrixFloats = 16;
constexpr int kTranslation = 12;
constexpr float kAxisX[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
constexpr int kMostBursts = 512;
constexpr size_t kHeader = 4;
constexpr int kKickKind = 6;
constexpr double kHalfFrame = 640.0;
constexpr double kAspect = 0.5625;
constexpr double kNearGround = 50.0;
constexpr double kRunSpeed = 10.0;
constexpr double kJump = 64.0;
constexpr int kWalkWait = 3;
constexpr int kRunWait = 2;
constexpr int kRunBursts = 3;
constexpr int kSides = 2;

struct Burst
{
	uint32_t start;
	float x;
	bool live;
};

struct Zone
{
	int limit;
	int kind;
};

struct Fighter
{
	bool known;
	bool grounded;
	double x;
	int walkWait;
	int runWait;
};

int g_first = 0;
int g_bursts = 0;
int g_perBurst = 0;
int g_frames = 0;
std::vector<Zone> g_zones;
Burst g_burst[kMostBursts] = {};
int g_next = 0;
uint32_t g_sceneFrame = 0;
uint32_t g_gameFrame = 0;
Fighter g_fighter[kSides] = {};
int g_placedStage = -1;
double g_scale = 0.0;
double g_offset = 0.0;
volatile long g_spawned = 0;
volatile long g_placed = 0;
char g_status[160] = "no stage has petals the fighters kick up";

int KindAt(double x)
{
	for (const Zone& zone : g_zones)
	{
		if (x < zone.limit)
			return zone.kind;
	}

	return 0;
}

bool Placed()
{
	const int stage = StagePlacement::Current();

	if (stage == g_placedStage)
		return g_scale != 0.0;

	g_placedStage = stage;
	g_scale = 0.0;

	StagePlacement::Place place = {};

	if (!StagePlacement::Of(stage, place) || place.scale[0] == 0.0f)
		return false;

	g_scale = place.scale[0] * kAspect;
	g_offset = place.position[0];

	return true;
}

void Kick(double x, int count)
{
	if (KindAt(x) != kKickKind || !Placed())
		return;

	const float model = static_cast<float>((x / kHalfFrame - g_offset) / g_scale);

	for (int i = 0; i < count; ++i)
	{
		g_burst[g_next] = { g_sceneFrame, model, true };
		g_next = (g_next + 1) % g_bursts;
		InterlockedIncrement(&g_spawned);
	}
}

void Step(Fighter& fighter, double x, double height)
{
	const bool grounded = height <= 0.0;
	const bool low = height < kNearGround;
	const double moved = std::fabs(x - fighter.x);
	const bool steady = fighter.known && moved < kJump;

	if (steady && grounded != fighter.grounded)
		Kick(x, 1);
	else if (steady && low && moved >= kRunSpeed && fighter.runWait <= 0)
	{
		Kick(x, kRunBursts);
		fighter.runWait = kRunWait;
	}
	else if (steady && low && moved > 0.0 && moved < kRunSpeed && fighter.walkWait <= 0)
	{
		Kick(x, 1);
		fighter.walkWait = kWalkWait;
	}

	fighter.known = true;
	fighter.grounded = grounded;
	fighter.x = x;
	fighter.walkWait = (std::max)(0, fighter.walkWait - 1);
	fighter.runWait = (std::max)(0, fighter.runWait - 1);
}

}

void StageKick::Hold(const std::vector<int>& kick)
{
	LOG("StageKick: %ld burst(s) kicked up and %ld placed on the last stage",
		InterlockedExchange(&g_spawned, 0), InterlockedExchange(&g_placed, 0));

	g_bursts = 0;
	g_zones.clear();
	memset(g_burst, 0, sizeof(g_burst));
	memset(g_fighter, 0, sizeof(g_fighter));
	g_next = 0;
	g_placedStage = -1;

	if (kick.size() < kHeader || kick[1] <= 0 || kick[1] > kMostBursts || kick[2] <= 0 || kick[3] < 2)
	{
		strncpy_s(g_status, "no stage has petals the fighters kick up", _TRUNCATE);
		return;
	}

	for (size_t i = kHeader; i + 1 < kick.size(); i += 2)
		g_zones.push_back({ kick[i], kick[i + 1] });

	g_first = kick[0];
	g_perBurst = kick[2];
	g_frames = kick[3];
	g_bursts = kick[1];

	sprintf_s(g_status, "%d burst(s) of %d petal(s) from node %d, %d zone(s)", g_bursts, g_perBurst, g_first,
		static_cast<int>(g_zones.size()));
	LOG("StageKick: %s", g_status);
}

void StageKick::Update()
{
	if (g_bursts == 0)
		return;

	uint32_t counter = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFrameCounterA)), counter)
		|| counter == g_gameFrame)
	{
		return;
	}

	g_gameFrame = counter;

	float scale = 0.0f;
	void* fighters[kSides] = {};
	const int count = MemoryMap::EnumerateCharaSlots(fighters, kSides, true);

	if (!Camera::GetPositionScale(scale))
		return;

	for (int i = 0; i < count; ++i)
	{
		int x = 0;
		int y = 0;

		if (Camera::GetWorldPosition(fighters[i], x, y))
			Step(g_fighter[i], x * scale, -y * scale);
	}
}

bool StageKick::Frame(int node, uint32_t& frame)
{
	g_sceneFrame = frame;

	if (g_bursts == 0 || node < g_first || node >= g_first + g_bursts * g_perBurst)
		return false;

	Burst& burst = g_burst[(node - g_first) / g_perBurst];
	const uint32_t age = frame - burst.start;

	burst.live = burst.live && age < static_cast<uint32_t>(g_frames - 1);
	frame = burst.live ? age : static_cast<uint32_t>(g_frames - 1);

	return true;
}

void StageKick::Place(void* scene, int node)
{
	const Burst& burst = g_burst[(node - g_first) / g_perBurst];

	if (!burst.live)
		return;

	uint8_t* const base = static_cast<uint8_t*>(scene);
	float* begin = nullptr;
	float* end = nullptr;
	memcpy(&begin, base + kStackBegin, sizeof(begin));
	memcpy(&end, base + kStackEnd, sizeof(end));

	if (begin == nullptr || end - begin < kMatrixFloats)
		return;

	float* const top = end - kMatrixFloats;
	const float* const parent = end - begin >= 2 * kMatrixFloats ? end - 2 * kMatrixFloats : kAxisX;

	for (int k = 0; k < 4; ++k)
		top[kTranslation + k] += burst.x * parent[k];

	InterlockedIncrement(&g_placed);
}
