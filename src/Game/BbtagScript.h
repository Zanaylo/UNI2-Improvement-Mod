#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace BbtagScript
{
	struct Rect
	{
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
	};

	struct Step
	{
		std::string take;
		int at;
	};

	struct Run
	{
		int loop;
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
		std::vector<Ramp> ramp;
	};

	typedef std::map<std::string, std::vector<uint8_t> > Scripts;

	bool Read(const std::vector<uint8_t>& blob, Sprite& out);

	bool ReadRun(const std::vector<uint8_t>& blob, Run& out);

	bool ReadLamp(const std::vector<uint8_t>& blob, Lamp& out);

	float LampAt(const Lamp& lamp, int frame);

	const Sprite* For(const std::map<std::string, Sprite>& held, const std::string& mesh);

	const Run* RunFor(const std::map<std::string, Run>& held, const std::string& mesh);
}
