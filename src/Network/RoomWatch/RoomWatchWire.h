#pragma once

#include "Game/Battle/MatchBlock.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/ConfirmedInputLog.h"
#include "Network/ModChannel.h"

#include <cstdint>

namespace RoomWatchWire
{
	constexpr uint16_t kVersion = 3;
	constexpr int kInputBytes = ConfirmedInputLog::kInputBytes;
	constexpr int kMostFramesPerBatch = 100;

	enum Type : uint8_t
	{
		Type_Join = 1,
		Type_Accepted,
		Type_Ready,
		Type_Added,
		Type_History,
		Type_Ack,
		Type_Leave,
		Type_Available
	};

#pragma pack(push, 1)
	struct Message
	{
		ModChannel::Header header;
		uint8_t type;
		uint8_t detail;
	};

	struct Accepted
	{
		Message message;
		int32_t seed;
		int32_t confirmed;
		int32_t localSide;
		int32_t remoteSide;
		int32_t block[MatchBlock::kSlots];
		uint8_t records[GameOffsets::kMatchRecordsSize];
	};

	struct Added
	{
		Message message;
		int32_t result;
		int32_t cursor;
	};

	struct History
	{
		Message message;
		int32_t first;
		uint16_t count;
		uint16_t bytes;
	};

	struct Ack
	{
		Message message;
		int32_t contiguous;
		int32_t firstLive;
	};
#pragma pack(pop)

	static_assert(sizeof(History) + kMostFramesPerBatch * kInputBytes <= ModChannel::kMaxBytes,
		"a history batch fits one mod message");

	inline Message Make(Type type, uint8_t detail)
	{
		Message message = {};
		message.header.magic = ModChannel::kMagic;
		message.header.version = kVersion;
		message.header.kind = ModChannel::kKindRoomWatch;
		message.type = type;
		message.detail = detail;

		return message;
	}
}
