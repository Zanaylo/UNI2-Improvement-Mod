#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace MbaaBg
{
	constexpr int kSpriteBase = 10000;
	constexpr int kFullParallax = 256;
	constexpr int kRecordValues = 6;

	enum Blend
	{
		Blend_Solid = 0,
		Blend_Add = 1,
		Blend_AddStrong = 2,
		Blend_Translucent = 3,
	};

	enum Op
	{
		Op_End = 0,
		Op_Next = 1,
		Op_Jump = 2,
		Op_NextToo = 3,
		Op_JumpToo = 4,
		Op_Count = 5,
	};

	enum Event
	{
		Event_Spawn = 1,
		Event_SpawnAnywhere = 2,
		Event_Velocity = 100,
	};

	enum Condition
	{
		Condition_Position = 1,
	};

	struct Record
	{
		int kind;
		int target;
		int values[kRecordValues];
	};

	struct Frame
	{
		int image;
		int x;
		int y;
		int duration;
		int blend;
		int alpha;
		int op;
		int jump;
		int tween;
		int exit;
		int loops;
		bool stopX;
		bool stopY;
		bool pushX;
		bool pushY;
		int velocity[2];
		int acceleration[2];
		std::vector<int> conditions;
		std::vector<int> events;
	};

	struct Layer
	{
		int object;
		int parallax;
		int priority;
		bool placed;
		std::vector<Frame> frames;
		std::vector<Record> events;
		std::vector<Record> conditions;
	};

	struct File
	{
		std::vector<Layer> layers;
		size_t cgAt = 0;
		size_t cgBytes = 0;
	};

	bool Read(const std::vector<uint8_t>& dat, File& out);

	const Layer* LayerOf(const File& file, int object);

	bool IsSprite(const Frame& frame);
	bool IsVisible(const Frame& frame);
	bool IsAdditive(const Frame& frame);
	int AlphaOf(const Frame& frame);
	float Opacity(const Frame& frame);
}
