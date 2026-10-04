#pragma once

#include "Game/Stages/Mbaa/MbaaBg.h"

#include <cstdint>
#include <vector>

class MbaaScene
{
public:
	struct Shown
	{
		int instance;
		int object;
		int image;
		float x;
		float y;
		int blend;
		int alpha;
	};

	MbaaScene(const MbaaBg::File& file, uint32_t seed);

	void Step();
	void Visible(std::vector<Shown>& out) const;
	uint64_t Hash() const;
	void Living(std::vector<int>& out) const;
	int Instances() const { return m_instances; }
	bool Rolled() const { return m_rolled; }

private:
	struct Actor
	{
		const MbaaBg::Layer* layer = nullptr;
		int state = 0;
		int frame = 0;
		int next = 0;
		int counter = 0;
		int timer = 0;
		int position[2] = {};
		int velocity[2] = {};
		int acceleration[2] = {};
		bool fired = false;
		bool pushed = false;
		int instance = 0;
	};

	void Start(Actor& actor, const MbaaBg::Layer& layer);
	void Enter(Actor& actor);
	void Advance(Actor& actor);
	void Move(Actor& actor);
	void Fire(Actor& actor);
	void Check(Actor& actor);
	void Spawn(const Actor& source, int object, int x, int y);
	void Velocity(Actor& actor, const MbaaBg::Record& event);
	int Roll(int low, int high);
	int AlphaNow(const Actor& actor) const;

	const MbaaBg::File& m_file;
	std::vector<Actor> m_actors;
	uint32_t m_seed;
	int m_instances = 0;
	bool m_rolled = false;
};
