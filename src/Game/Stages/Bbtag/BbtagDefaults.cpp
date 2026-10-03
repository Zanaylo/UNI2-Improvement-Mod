#include "Game/Stages/Bbtag/BbtagDefaults.h"

#include <cstring>

namespace {

const BbtagDefaults::Look kBbtagLooks[] = {
	{ "bg_beach", 1.00f, 1.11f, 1.00f },
	{ "bg_bluegate", 1.00f, 1.10f, 1.05f },
	{ "bg_boss", 1.05f, 1.15f, 1.10f },
	{ "bg_church_2", 1.10f, 1.30f, 1.21f },
	{ "bg_crater", 1.00f, 1.20f, 1.05f },
	{ "bg_crossfield", 1.10f, 1.25f, 1.00f },
	{ "bg_entrance", 1.10f, 1.10f, 1.00f },
	{ "bg_falling_blossoms", 1.05f, 1.20f, 1.10f },
	{ "bg_foodcourt", 1.05f, 1.25f, 1.15f },
	{ "bg_fountain_plaza", 1.00f, 1.25f, 1.00f },
	{ "bg_garden", 1.05f, 1.15f, 1.08f },
	{ "bg_gate_p4u2", 1.00f, 1.10f, 1.10f },
	{ "bg_gyoenjogakuen", 1.00f, 1.10f, 1.05f },
	{ "bg_intersection", 1.05f, 1.15f, 1.10f },
	{ "bg_ishana", 1.10f, 1.25f, 1.20f },
	{ "bg_lakeside", 1.15f, 1.25f, 1.10f },
	{ "bg_monolis", 1.00f, 1.20f, 1.16f },
	{ "bg_nevermore", 1.05f, 1.20f, 1.10f },
	{ "bg_odaiba", 1.07f, 1.20f, 0.90f },
	{ "bg_prologue", 1.10f, 1.10f, 1.10f },
	{ "bg_ring", 1.15f, 1.15f, 1.00f },
	{ "bg_snowtown", 1.10f, 1.26f, 1.20f },
	{ "bg_stadium", 1.00f, 1.25f, 1.05f },
	{ "bg_station", 1.10f, 1.32f, 1.14f },
	{ "bg_station_2", 1.10f, 1.25f, 1.05f },
	{ "bg_street", 1.05f, 1.25f, 1.20f },
	{ "bg_test_2", 0.90f, 1.10f, 1.00f },
	{ "bg_test_3", 1.00f, 1.15f, 4.00f },
	{ "bg_test_4", 40.00f, 1.20f, 1.05f },
	{ "bg_test_5", 25.00f, 1.10f, 1.00f },
	{ "bg_town", 1.10f, 1.28f, 1.10f },
	{ "bg_training", 1.10f, 1.15f, 1.10f },
};

const BbtagDefaults::Look kP4u2Looks[] = {
	{ "bg_astro", 1.00f, 1.35f, 1.00f },
	{ "bg_boss_1", 1.00f, 1.17f, 1.00f },
	{ "bg_boss_1_p4u2", 1.00f, 1.17f, 1.00f },
	{ "bg_boss_2", 1.00f, 1.15f, 1.00f },
	{ "bg_boss_2_p4u2", 1.00f, 1.15f, 1.00f },
	{ "bg_boss_3_a", 1.00f, 1.30f, 1.00f },
	{ "bg_boss_3_b", 1.00f, 1.17f, 1.00f },
	{ "bg_classroom", 1.00f, 1.26f, 1.00f },
	{ "bg_classroom_p4u2", 1.00f, 1.26f, 1.00f },
	{ "bg_corridor", 1.00f, 1.17f, 1.00f },
	{ "bg_corridor_2", 1.00f, 1.14f, 1.00f },
	{ "bg_corridor_p4u2", 1.00f, 1.17f, 1.00f },
	{ "bg_entrance", 1.00f, 1.17f, 1.00f },
	{ "bg_entrance_p4u2", 1.00f, 1.17f, 1.00f },
	{ "bg_foodcourt", 1.00f, 1.18f, 1.00f },
	{ "bg_gate", 1.00f, 1.13f, 1.00f },
	{ "bg_gate_3", 1.00f, 1.20f, 1.00f },
	{ "bg_gate_night", 1.00f, 1.08f, 1.00f },
	{ "bg_gate_night_p4u2", 1.00f, 1.08f, 1.00f },
	{ "bg_gate_p4u2", 1.00f, 1.13f, 1.00f },
	{ "bg_gym", 1.00f, 1.15f, 1.00f },
	{ "bg_gym_p4u2", 1.00f, 1.15f, 1.00f },
	{ "bg_gym_p4u2_b", 1.00f, 1.14f, 1.00f },
	{ "bg_musicroom", 1.00f, 1.17f, 1.00f },
	{ "bg_musicroom_p4u2", 1.00f, 1.17f, 1.00f },
	{ "bg_riverbed", 1.00f, 1.16f, 1.00f },
	{ "bg_street", 1.00f, 1.25f, 1.00f },
	{ "bg_street_2", 1.00f, 1.19f, 1.00f },
	{ "bg_street_p4u2", 1.00f, 1.25f, 1.00f },
	{ "bg_tartaros", 1.00f, 1.21f, 1.00f },
	{ "bg_town", 1.00f, 1.16f, 1.00f },
};

template <size_t N>
const BbtagDefaults::Look* Find(const BbtagDefaults::Look (&looks)[N], const std::string& stage)
{
	for (const BbtagDefaults::Look& look : looks)
	{
		if (_stricmp(look.stage, stage.c_str()) == 0)
			return &look;
	}

	return nullptr;
}

}

const BbtagDefaults::Look* BbtagDefaults::Of(FbGameFolder::Game game, const std::string& stage)
{
	if (game == FbGameFolder::Game_P4U2)
		return Find(kP4u2Looks, stage);

	return game == FbGameFolder::Game_BBTAG ? Find(kBbtagLooks, stage) : nullptr;
}
