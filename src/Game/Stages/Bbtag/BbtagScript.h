#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace BbtagScript
{
	struct Rect
	{
		int sheet;
		int x;
		int y;
		int w;
		int h;
	};

	struct Sprite
	{
		int loop;
		std::vector<int> frame;
		std::vector<Rect> rect;
		std::vector<std::string> sheets;
	};

	struct Step
	{
		std::string take;
		int at;
	};

	struct Run
	{
		int loop;
		bool settled;
		std::vector<Step> frame;
	};

	struct Ramp
	{
		int at;
		int target;
		int frames;
	};

	struct Lamp
	{
		int loop;
		int from;
		std::vector<Ramp> ramp;
	};

	struct Flip
	{
		std::vector<float> rects;
		std::vector<int> frame;
	};

	struct Sample
	{
		bool lit;
		Rect rect;
		double ramp;
		bool picked;
		int64_t pick;
	};

	struct Played
	{
		std::vector<Sample> sample;
		std::vector<std::string> sheets;
		std::vector<std::string> named;
		bool cyclic;
		bool rolled;
		int from;
	};

	struct Spawn
	{
		std::string effect;
		int bone;
	};

	typedef std::map<std::string, std::vector<uint8_t> > Scripts;

	bool Play(const std::vector<uint8_t>& blob, const std::string& label, Played& out);

	bool Sprites(const Played& played, Sprite& out);

	bool Motions(const Played& played, Run& out);

	bool Lamps(const Played& played, Lamp& out);

	bool Showing(const Played& played, const Rect& rect, Lamp& out);

	bool Long(const Played& played);

	bool Tilt(const std::vector<uint8_t>& blob, float& degrees);

	bool Spawns(const std::vector<uint8_t>& blob, std::vector<Spawn>& out);
}
