#include "Palette/NetworkPickRule.h"

namespace {

bool IsValid(const NetworkPickRule::Profile& game, const NetworkPickRule::Limits& limits)
{
	return game.chara >= 0 && game.chara < limits.characters && game.colour >= 0 && game.colour < limits.colours;
}

bool Matches(const NetworkPickRule::Pick& pick, const NetworkPickRule::Profile& game)
{
	return pick.chara == game.chara && pick.colour == game.colour;
}

}

NetworkPickRule::Action NetworkPickRule::Decide(const Pick& pick, const Profile& game, const Limits& limits, bool seated)
{
	if (!IsValid(game, limits))
		return Action::Keep;

	if (!pick.present)
		return Action::Adopt;

	if (pick.hasSignature)
		return Action::Keep;

	if (!Matches(pick, game))
		return Action::Adopt;

	return seated ? Action::Sign : Action::Keep;
}

bool NetworkPickRule::Disagrees(const Pick& pick, const Profile& game, const Limits& limits)
{
	return pick.present && IsValid(game, limits) && !Matches(pick, game);
}
