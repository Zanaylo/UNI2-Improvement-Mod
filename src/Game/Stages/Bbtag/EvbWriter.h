#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace EvbWriter
{
	constexpr uint32_t kBegin = 0x01;
	constexpr uint32_t kYield = 0x02;
	constexpr uint32_t kWait = 0x03;
	constexpr uint32_t kClose = 0x04;
	constexpr uint32_t kPick = 0x06;
	constexpr uint32_t kSceneOpen = 0x1a;
	constexpr uint32_t kSceneClose = 0x1b;
	constexpr uint32_t kSceneVector = 0x1d;
	constexpr uint32_t kScenePair = 0x1e;
	constexpr uint32_t kSceneHeight = 0x1f;
	constexpr uint32_t kSceneSwitches = 0x21;
	constexpr uint32_t kSceneTilt = 0x28;

	struct Record
	{
		uint32_t code;
		std::vector<int32_t> operands;
	};

	struct Script
	{
		std::vector<std::string> sheets;
		std::vector<std::string> names;
		std::vector<Record> records;
	};

	void Build(const Script& script, std::vector<uint8_t>& out);
}
