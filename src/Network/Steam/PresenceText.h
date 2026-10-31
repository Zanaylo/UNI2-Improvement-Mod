#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace PresenceText
{
	constexpr size_t kMaxValueBytes = 255;

	enum class Place
	{
		Menus,
		CharacterSelect,
		Training,
		Offline,
		Replays,
		Room,
		OnlineMatch,
		Spectating
	};

	constexpr int kSides = 2;

	struct Fighter
	{
		std::string name;
		std::string character;
	};

	struct Situation
	{
		Place place = Place::Menus;
		std::string localCharacter;
		Fighter fighter[kSides];
		std::string note;
	};

	std::string Status(const Situation& situation);

	std::string Group(uint64_t lobby);
}
