#include "Game/Replays/LiveRecording.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Replays/ReplayRecord.h"

#include <Windows.h>

namespace {

constexpr int32_t kWriterUnset = -1;

struct GameWriter
{
	int32_t mode;
	int32_t cursor;
	uint8_t* begin;
	uint8_t* end;
	uint8_t* capacity;
};

typedef int(__fastcall* SerializeFn)(void* recorder, void* unused, GameWriter* writer);
typedef void(__fastcall* FreeBytesFn)(uint8_t** vector);

uintptr_t Recorder()
{
	return RvaToAddress(GameOffsets::kReplayLoadObject);
}

bool ReadRounds(uint32_t& outBegin, uint32_t& outEnd)
{
	const uintptr_t recorder = Recorder();

	return recorder != 0 &&
		TryReadDword(reinterpret_cast<const void*>(recorder + GameOffsets::kReplayRoundsBegin), outBegin) &&
		TryReadDword(reinterpret_cast<const void*>(recorder + GameOffsets::kReplayRoundsEnd), outEnd);
}

bool SerializeGuarded(SerializeFn serialize, void* recorder, GameWriter* writer)
{
	__try
	{
		return serialize(recorder, nullptr, writer) != 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

void FreeGuarded(FreeBytesFn release, GameWriter* writer)
{
	__try
	{
		release(&writer->begin);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

bool CopyWritten(const GameWriter& writer, std::vector<uint8_t>& out)
{
	if (writer.begin == nullptr || writer.end <= writer.begin)
		return false;

	const size_t size = static_cast<size_t>(writer.end - writer.begin);

	if (size > ReplayRecord::kMaxPayload)
		return false;

	out.resize(size);
	return TryReadMemory(out.data(), writer.begin, size);
}

bool IsTakeState(uint32_t wanted)
{
	uint32_t state = 0;
	const uintptr_t address = RvaToAddress(GameOffsets::kReplayTakeState);

	return address != 0 && TryReadDword(reinterpret_cast<const void*>(address), state) && state == wanted;
}

}

bool LiveRecording::IsRecording()
{
	return IsTakeState(GameOffsets::kReplayTakeRecording);
}

bool LiveRecording::IsPlaying()
{
	return IsTakeState(GameOffsets::kReplayTakePlaying);
}

int LiveRecording::RoundCount()
{
	uint32_t begin = 0;
	uint32_t end = 0;

	if (!ReadRounds(begin, end) || begin == 0 || end < begin)
		return 0;

	return static_cast<int>((end - begin) / GameOffsets::kReplayRoundStride);
}

bool LiveRecording::Capture(std::vector<uint8_t>& outPayload)
{
	outPayload.clear();

	const uintptr_t serialize = CodeSignatures::Address(GameOffsets::kFnSerializeReplay);
	const uintptr_t release = CodeSignatures::Address(GameOffsets::kFnFreeByteVector);
	const uintptr_t recorder = Recorder();

	if (serialize == 0 || release == 0 || recorder == 0)
	{
		LOG("live recording: the replay writer is not where this game version expects it");
		return false;
	}

	if (RoundCount() <= 0)
	{
		LOG("live recording: nothing has been recorded yet");
		return false;
	}

	GameWriter writer = { kWriterUnset, 0, nullptr, nullptr, nullptr };

	const bool written = SerializeGuarded(reinterpret_cast<SerializeFn>(serialize),
		reinterpret_cast<void*>(recorder), &writer);
	const bool copied = written && CopyWritten(writer, outPayload);

	FreeGuarded(reinterpret_cast<FreeBytesFn>(release), &writer);

	LOG("live recording: %d round(s), serialised %s, %zu bytes", RoundCount(),
		written ? "ok" : "FAILED", outPayload.size());

	if (!copied)
		outPayload.clear();

	return copied;
}
