#include "Training/StateRecorder.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/CharaTracker.h"
#include "Game/GameOffsets.h"
#include "Game/MemoryMap.h"
#include "Game/PlayerState.h"

#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kMaxEntities = 4;
constexpr int kMaxRecords = 600 * 1024;
constexpr int kSnapshotDwords = GameOffsets::kPlayerDataSize / 4;
constexpr int kEntityRefreshFrames = 30;

enum RecordKind : uint8_t
{
	Record_State = 0,
	Record_Delta = 1
};

struct Record
{
	uint32_t frame;
	uint8_t entity;
	uint8_t kind;
	uint16_t offset;
	uint32_t a;
	uint32_t b;
	uint16_t pattern;
	uint16_t frameIndex;

	uint32_t moveCode;
	uint32_t moveCodeEx2;
	uint32_t moveCodeEx3;
	uint32_t moveCodeEx7;
	uint32_t attrInvuln;
	uint32_t actionable;
	uint32_t actionLock;
	uint32_t packed;
};

uint32_t PackState(const PlayerState::State& state)
{
	return static_cast<uint32_t>(state.hurtboxCount) |
		(static_cast<uint32_t>(state.invulnKind) << 8) |
		(state.atemiWindow ? 1u << 16 : 0u) |
		(state.airJumpOK != 0 ? 1u << 17 : 0u) |
		(state.running != 0 ? 1u << 18 : 0u) |
		(state.existNoKurai ? 1u << 19 : 0u) |
		(state.counterWindow ? 1u << 20 : 0u) |
		(static_cast<uint32_t>(PlayerState::GetStance(state)) << 21);
}

Record* g_records = nullptr;
uint32_t* g_snapshots = nullptr;
bool g_snapshotValid[kMaxEntities] = {};

void* g_entities[kMaxEntities] = {};
int g_entityCount = 0;

volatile long g_recording = 0;
bool g_includeDeltas = false;
bool g_bufferFull = false;

int g_recordCount = 0;
uint32_t g_frame = 0;
int g_framesUntilRefresh = 0;

int g_takeIndex = 0;

char g_lastPath[MAX_PATH] = {};

bool EnsureBuffers()
{
	if (g_records != nullptr)
		return true;

	g_records = static_cast<Record*>(malloc(sizeof(Record) * kMaxRecords));
	g_snapshots = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * kSnapshotDwords * kMaxEntities));

	if (g_records == nullptr || g_snapshots == nullptr)
	{
		LOG("StateRecorder: could not allocate capture buffers");
		free(g_records);
		free(g_snapshots);
		g_records = nullptr;
		g_snapshots = nullptr;
		return false;
	}

	return true;
}

Record* Append(uint8_t entity, uint8_t kind, uint16_t offset, uint32_t a, uint32_t b,
	uint16_t pattern, uint16_t frameIndex)
{
	if (g_recordCount >= kMaxRecords)
	{
		g_bufferFull = true;
		return nullptr;
	}

	Record& record = g_records[g_recordCount++];
	record.frame = g_frame;
	record.entity = entity;
	record.kind = kind;
	record.offset = offset;
	record.a = a;
	record.b = b;
	record.pattern = pattern;
	record.frameIndex = frameIndex;
	record.moveCode = 0;
	record.moveCodeEx2 = 0;
	record.moveCodeEx3 = 0;
	record.moveCodeEx7 = 0;
	record.attrInvuln = 0;
	record.actionable = 0;
	record.actionLock = 0;
	record.packed = 0;
	return &record;
}

void RefreshEntities()
{
	void* candidates[16] = {};
	int found = 0;

	CharaTracker::Entry entry = {};
	for (int i = 0; i < CharaTracker::GetEntryCount(); ++i)
	{
		if (!CharaTracker::GetEntry(i, entry))
			continue;

		if (!MemoryMap::IsPlayerData(entry.charaData))
			continue;

		found = MemoryMap::EnumeratePlayerData(entry.charaData, candidates, 16);
		if (found > 0)
			break;
	}

	int kept = 0;
	for (int i = 0; i < found && kept < kMaxEntities; ++i)
	{
		uint32_t context = 0;
		if (!MemoryMap::ReadStructDword(candidates[i], GameOffsets::kCharaFrameObject, context))
			continue;

		if (context == 0)
			continue;

		if (g_entities[kept] != candidates[i])
			g_snapshotValid[kept] = false;

		g_entities[kept] = candidates[i];
		++kept;
	}

	for (int i = kept; i < kMaxEntities; ++i)
	{
		g_entities[i] = nullptr;
		g_snapshotValid[i] = false;
	}

	g_entityCount = kept;
}

void SampleDeltas(int index, void* entity, uint16_t pattern, uint16_t frameIndex)
{
	uint32_t* previous = g_snapshots + static_cast<size_t>(index) * kSnapshotDwords;

	uint32_t current[kSnapshotDwords];
	if (!TryReadMemory(current, entity, sizeof(current)))
		return;

	if (!g_snapshotValid[index])
	{
		memcpy(previous, current, sizeof(current));
		g_snapshotValid[index] = true;
		return;
	}

	for (int i = 0; i < kSnapshotDwords; ++i)
	{
		if (current[i] == previous[i])
			continue;

		Append(static_cast<uint8_t>(index), Record_Delta, static_cast<uint16_t>(i * 4),
			previous[i], current[i], pattern, frameIndex);
		previous[i] = current[i];
	}
}

}

bool StateRecorder::IsRecording()
{
	return g_recording != 0;
}

bool StateRecorder::IncludesDeltas()
{
	return g_includeDeltas;
}

bool StateRecorder::IsBufferFull()
{
	return g_bufferFull;
}

int StateRecorder::GetSampledFrames()
{
	return static_cast<int>(g_frame);
}

int StateRecorder::GetRecordCount()
{
	return g_recordCount;
}

int StateRecorder::GetCapacity()
{
	return kMaxRecords;
}

int StateRecorder::GetTrackedEntities()
{
	return g_entityCount;
}

const char* StateRecorder::GetLastFilePath()
{
	return g_lastPath;
}

void StateRecorder::Start(bool includeDeltas)
{
	if (g_recording != 0 || !EnsureBuffers())
		return;

	g_includeDeltas = includeDeltas;
	g_bufferFull = false;
	g_recordCount = 0;
	g_frame = 0;
	g_entityCount = 0;
	g_framesUntilRefresh = 0;

	for (int i = 0; i < kMaxEntities; ++i)
	{
		g_entities[i] = nullptr;
		g_snapshotValid[i] = false;
	}

	InterlockedExchange(&g_recording, 1);
	LOG("StateRecorder started (deltas=%d)", includeDeltas ? 1 : 0);
}

void StateRecorder::SampleFromGameThread()
{
	if (g_recording == 0)
		return;

	if (--g_framesUntilRefresh <= 0)
	{
		RefreshEntities();
		g_framesUntilRefresh = kEntityRefreshFrames;
	}

	for (int i = 0; i < g_entityCount; ++i)
	{
		void* entity = g_entities[i];

		PlayerState::State state = {};
		if (!PlayerState::Read(entity, state))
			continue;

		Record* row = Append(static_cast<uint8_t>(i), Record_State, 0, state.mvCountFrame,
			static_cast<uint32_t>(state.attackBoxes), state.pattern, state.frameIndex);

		if (row != nullptr)
		{
			row->moveCode = state.moveCodeEx[0];
			row->moveCodeEx2 = state.moveCodeEx[2];
			row->moveCodeEx3 = state.moveCodeEx[3];
			row->moveCodeEx7 = state.moveCodeEx[7];
			row->attrInvuln = state.attrInvuln;
			row->actionable = state.actionableRaw;
			row->actionLock = state.actionLock;
			row->packed = PackState(state);
		}

		if (g_includeDeltas)
			SampleDeltas(i, entity, state.pattern, state.frameIndex);
	}

	++g_frame;
}

void StateRecorder::Stop()
{
	if (g_recording == 0)
		return;

	InterlockedExchange(&g_recording, 0);

	char take[8] = {};
	sprintf_s(take, "_%02d", ++g_takeIndex);

	const std::string path = GetModLogPath("UNI2_IM_state_" + GetLogSessionStamp() + take + ".csv");
	strncpy_s(g_lastPath, path.c_str(), _TRUNCATE);

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "w") != 0 || file == nullptr)
	{
		LOG("StateRecorder: could not open %s", path.c_str());
		return;
	}

	fprintf(file, "frame,entity,kind,offset,a,b,pattern,frameIndex,moveCode,moveCodeEx2,"
		"moveCodeEx3,moveCodeEx7,attrInvuln,actionable,actionLock,hurtboxes,invulnKind,atemi,"
		"airJumpOK,running,noKurai,counterWindow,stance\n");

	for (int i = 0; i < g_recordCount; ++i)
	{
		const Record& record = g_records[i];

		if (record.kind == Record_State)
		{
			fprintf(file, "%u,%u,state,,%u,%d,%u,%u,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,%u,0x%08x,"
				"%u,%u,%u,%u,%u,%u,%u,%u\n",
				record.frame, record.entity, record.a, static_cast<int>(record.b),
				record.pattern, record.frameIndex, record.moveCode, record.moveCodeEx2,
				record.moveCodeEx3, record.moveCodeEx7, record.attrInvuln, record.actionable,
				record.actionLock,
				record.packed & 0xff, (record.packed >> 8) & 0xff,
				(record.packed >> 16) & 1, (record.packed >> 17) & 1, (record.packed >> 18) & 1,
				(record.packed >> 19) & 1, (record.packed >> 20) & 1, (record.packed >> 21) & 3);
		}
		else
		{
			fprintf(file, "%u,%u,delta,0x%03x,%u,%u,%u,%u,,,,,,,,,,,,,,\n",
				record.frame, record.entity, record.offset, record.a, record.b,
				record.pattern, record.frameIndex);
		}
	}

	fclose(file);
	LOG("StateRecorder wrote %d records over %u frames to %s", g_recordCount, g_frame, path.c_str());
}
