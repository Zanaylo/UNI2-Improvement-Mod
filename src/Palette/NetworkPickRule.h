#pragma once

namespace NetworkPickRule
{
	enum class Action
	{
		Keep,
		Adopt,
		Sign
	};

	struct Pick
	{
		bool present;
		int chara;
		int colour;
		bool hasSignature;
	};

	struct Profile
	{
		int chara;
		int colour;
	};

	struct Limits
	{
		int characters;
		int colours;
	};

	Action Decide(const Pick& pick, const Profile& game, const Limits& limits, bool seated);
	bool Disagrees(const Pick& pick, const Profile& game, const Limits& limits);
}
