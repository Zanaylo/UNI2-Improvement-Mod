#pragma once

#include "Network/ModChannel.h"
#include "Network/SpectateMatch.h"

#include <cstdint>

namespace SpectateWire
{
	constexpr uint16_t kVersion = 5;
	constexpr const char* kPresenceKey = "uni2im_spectate";

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
		Type_MatchEnd
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
