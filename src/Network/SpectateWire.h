#pragma once

#include "Network/ModChannel.h"
#include "Network/SpectateMatch.h"

#include <cstdint>

namespace SpectateWire
{
	constexpr uint16_t kVersion = 6;
	constexpr const char* kPresenceKey = "uni2im_spectate";

	constexpr int kInputBytes = 36;
	constexpr int kMostFramesPerBatch = 60;

	enum Type : uint8_t
	{
		Type_Join = 1,
		Type_Accepted,
		Type_Pending,
		Type_Refused,
		Type_Kicked,
		Type_Armed,
		Type_Leave,
		Type_MatchStart,
		Type_Behind,
		Type_MatchEnd,
		Type_Inputs,
		Type_Ack
	};

	enum Refusal : uint8_t
	{
		Refusal_None = 0,
		Refusal_Closed,
		Refusal_Full,
		Refusal_Declined
	};

#pragma pack(push, 1)
	struct Message
	{
		ModChannel::Header header;
		uint8_t type;
		uint8_t detail;
	};

	struct MatchStart
	{
		Message message;
		SpectateMatch::Snapshot snapshot;
	};

	struct Inputs
	{
		Message message;
		int32_t first;
		uint16_t count;
		uint16_t bytes;
	};

	struct Ack
	{
		Message message;
		int32_t frame;
	};
#pragma pack(pop)

	inline Message Make(Type type, uint8_t detail)
	{
		Message message = {};
		message.header.magic = ModChannel::kMagic;
		message.header.version = kVersion;
		message.header.kind = ModChannel::kKindSpectate;
		message.type = type;
		message.detail = detail;

		return message;
	}
}
