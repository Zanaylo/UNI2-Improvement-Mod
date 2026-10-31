#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace BgmTableSites
{
	struct Table
	{
		uint32_t base;
		uint32_t stride;
		int slots;
	};

	struct Plan
	{
		std::vector<size_t> fields;
		std::vector<size_t> ends;
		std::vector<size_t> bounds;
		std::vector<size_t> clears;
	};

	Plan Find(const uint8_t* code, size_t size, const Table& stock);

	bool Expected(const Plan& plan);

	uint32_t Field(uint32_t value, const Table& from, const Table& to);
	uint32_t End(uint32_t value, const Table& from, const Table& to);
	uint32_t Bound(uint32_t value, const Table& from, const Table& to);
	uint32_t Clear(const Table& to);
}
