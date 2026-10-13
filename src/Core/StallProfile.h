#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace StallProfile
{
	constexpr int kChainDepth = 3;
	constexpr int kMostSites = 48;

	enum Leaf : uint8_t
	{
		Leaf_Game,
		Leaf_Kernel,
		Leaf_User,
		Leaf_Direct3D,
		Leaf_Mod,
		Leaf_Other,
		Leaf_COUNT
	};

	struct Image
	{
		uint32_t base;
		uint32_t codeStart;
		uint32_t codeSize;
		const uint8_t* code;
	};

	struct Site
	{
		uint32_t chain[kChainDepth];
		int depth;
		Leaf leaf;
		uint32_t samples;
	};

	bool FollowsCall(const Image& image, uint32_t address);
	int BuildChain(const Image& image, uint32_t instruction, const uint32_t* stack, size_t words,
		uint32_t (&chain)[kChainDepth]);

	class Table
	{
	public:
		void Add(const uint32_t* chain, int depth, Leaf leaf);
		void Clear();

		uint32_t Samples() const;
		int Used() const;
		uint32_t Dropped() const;
		std::vector<Site> Top(int count) const;

	private:
		Site m_sites[kMostSites] = {};
		int m_used = 0;
		uint32_t m_samples = 0;
		uint32_t m_dropped = 0;
	};

	std::string Describe(const Table& table, int count);
}
