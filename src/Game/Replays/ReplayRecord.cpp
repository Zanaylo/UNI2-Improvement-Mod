#include "Game/Replays/ReplayRecord.h"

#include <cstring>

namespace {

template <typename T>
void WriteAt(std::vector<uint8_t>& record, size_t offset, const T& value)
{
	memcpy(record.data() + offset, &value, sizeof(value));
}

void WritePlayer(std::vector<uint8_t>& record, int player, const ReplayRecord::Player& facts)
{
	const size_t block = ReplayRecord::PlayerBlock(player);
	const uint32_t chara = static_cast<uint32_t>(facts.chara);

	WriteAt(record, block - ReplayRecord::kPlayerSteamIdBack, facts.steamId);
	WriteAt(record, block + ReplayRecord::kPlayerChara, chara);
}

}

bool ReplayRecord::FromPayload(const std::vector<uint8_t>& payload, const Facts& facts,
	std::vector<uint8_t>& outRecord)
{
	if (payload.empty() || payload.size() > kMaxPayload)
		return false;

	std::vector<uint8_t> record(kSize, 0);
	const uint32_t length = static_cast<uint32_t>(payload.size());

	memcpy(record.data() + kOffPayload, payload.data(), payload.size());
	WriteAt(record, kOffPayloadSize, length);
	WriteAt(record, kOffVersion, facts.version);
	WriteAt(record, kOffAuthorId, facts.author);
	WriteAt(record, kOffTime, facts.time);
	WriteAt(record, kOffTailTime, facts.time);

	for (int player = 0; player < kPlayers; ++player)
		WritePlayer(record, player, facts.player[player]);

	outRecord.swap(record);
	return true;
}

bool ReplayRecord::HasValidLength(const std::vector<uint8_t>& record)
{
	if (record.size() != kSize)
		return false;

	uint32_t length = 0;
	memcpy(&length, record.data() + kOffPayloadSize, sizeof(length));

	return length != 0 && length <= kMaxPayload;
}
