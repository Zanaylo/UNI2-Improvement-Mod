#include "Network/Steam/PresenceText.h"

namespace {

constexpr unsigned char kUtf8ContinuationMask = 0xc0;
constexpr unsigned char kUtf8Continuation = 0x80;

std::string Playing(const char* activity, const std::string& character)
{
	return character.empty() ? activity : std::string(activity) + " as " + character;
}

bool IsUnknown(const PresenceText::Fighter& fighter)
{
	return fighter.name.empty() && fighter.character.empty();
}

std::string Label(const PresenceText::Fighter& fighter)
{
	if (IsUnknown(fighter))
		return "?";

	if (fighter.name.empty())
		return fighter.character;

	if (fighter.character.empty())
		return fighter.name;

	return fighter.name + "(" + fighter.character + ")";
}

std::string OnlineMatch(const PresenceText::Situation& situation)
{
	const PresenceText::Fighter& left = situation.fighter[0];
	const PresenceText::Fighter& right = situation.fighter[1];

	if (IsUnknown(left) && IsUnknown(right))
		return "In an online match";

	return Label(left) + " vs " + Label(right);
}

std::string Describe(const PresenceText::Situation& situation)
{
	using PresenceText::Place;

	switch (situation.place)
	{
	case Place::CharacterSelect:
		return "Choosing a character";
	case Place::Training:
		return Playing("Training", situation.localCharacter);
	case Place::Offline:
		return Playing("Playing offline", situation.localCharacter);
	case Place::Replays:
		return "Watching replays";
	case Place::Room:
		return "In a Player Match room";
	case Place::OnlineMatch:
		return OnlineMatch(situation);
	case Place::Spectating:
		return "Watching an online match";
	default:
		return "In the menus";
	}
}

bool IsContinuationByte(char c)
{
	return (static_cast<unsigned char>(c) & kUtf8ContinuationMask) == kUtf8Continuation;
}

std::string FitBytes(const std::string& text, size_t limit)
{
	if (text.size() <= limit)
		return text;

	size_t end = limit;

	while (end > 0 && IsContinuationByte(text[end]))
		--end;

	return text.substr(0, end);
}

}

std::string PresenceText::Status(const Situation& situation)
{
	const std::string text = Describe(situation);

	return FitBytes(situation.note.empty() ? text : text + " - " + situation.note, kMaxValueBytes);
}

std::string PresenceText::Group(uint64_t lobby)
{
	return lobby == 0 ? std::string() : std::to_string(lobby);
}
