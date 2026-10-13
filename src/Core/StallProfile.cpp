#include "Core/StallProfile.h"

#include <algorithm>
#include <cstdio>

namespace {

constexpr uint8_t kCallRel32 = 0xE8;
constexpr uint8_t kGroupFive = 0xFF;
constexpr uint8_t kModRmMod = 0xC0;
constexpr uint8_t kModRmReg = 0x38;
constexpr uint8_t kModRmRm = 0x07;
constexpr uint8_t kCallNear = 0x10;
constexpr uint8_t kModRegister = 0xC0;
constexpr uint8_t kModMemory = 0x00;
constexpr uint8_t kModDisp8 = 0x40;
constexpr uint8_t kModDisp32 = 0x80;
constexpr uint8_t kRmSib = 0x04;
constexpr uint8_t kRmAbsolute = 0x05;
constexpr size_t kLongestCall = 6;

const char* const kLeafNames[StallProfile::Leaf_COUNT] = {
	"game", "kernel", "user32", "d3d", "mod", "other",
};

bool IsCallModRm(uint8_t modRm, uint8_t mod)
{
	return (modRm & kModRmReg) == kCallNear && (modRm & kModRmMod) == mod;
}

bool IsCallThroughRegister(uint8_t modRm)
{
	return IsCallModRm(modRm, kModRegister);
}

bool IsCallThroughRegisterPointer(uint8_t modRm)
{
	const uint8_t rm = modRm & kModRmRm;

	return IsCallModRm(modRm, kModMemory) && rm != kRmSib && rm != kRmAbsolute;
}

bool IsCallThroughAbsolute(uint8_t modRm)
{
	return IsCallModRm(modRm, kModMemory) && (modRm & kModRmRm) == kRmAbsolute;
}

bool IsVirtualCall(uint8_t modRm, uint8_t mod)
{
	return IsCallModRm(modRm, mod) && (modRm & kModRmRm) != kRmSib;
}

bool Contains(const StallProfile::Image& image, uint32_t address)
{
	return address >= image.codeStart && address - image.codeStart < image.codeSize;
}

bool SameChain(const StallProfile::Site& site, const uint32_t* chain, int depth, StallProfile::Leaf leaf)
{
	if (site.leaf != leaf || site.depth != depth)
		return false;

	return std::equal(chain, chain + depth, site.chain);
}

std::string SiteText(const StallProfile::Site& site)
{
	char text[96] = {};
	int length = sprintf_s(text, "%u in %s at", site.samples, kLeafNames[site.leaf]);

	for (int i = 0; i < site.depth; ++i)
		length += sprintf_s(text + length, sizeof(text) - length, "%s0x%06x", i == 0 ? " " : " < ", site.chain[i]);

	return text;
}

}

bool StallProfile::FollowsCall(const Image& image, uint32_t address)
{
	if (!Contains(image, address - 1) || address - image.codeStart < kLongestCall)
		return false;

	const uint8_t* const after = image.code + (address - image.codeStart);

	if (after[-5] == kCallRel32)
		return true;

	if (after[-6] == kGroupFive && (IsCallThroughAbsolute(after[-5]) || IsVirtualCall(after[-5], kModDisp32)))
		return true;

	if (after[-3] == kGroupFive && IsVirtualCall(after[-2], kModDisp8))
		return true;

	return after[-2] == kGroupFive && (IsCallThroughRegister(after[-1]) || IsCallThroughRegisterPointer(after[-1]));
}

int StallProfile::BuildChain(const Image& image, uint32_t instruction, const uint32_t* stack, size_t words,
	uint32_t (&chain)[kChainDepth])
{
	int depth = 0;

	if (Contains(image, instruction))
		chain[depth++] = instruction - image.base;

	for (size_t i = 0; i < words && depth < kChainDepth; ++i)
	{
		if (FollowsCall(image, stack[i]))
			chain[depth++] = stack[i] - image.base;
	}

	return depth;
}

void StallProfile::Table::Add(const uint32_t* chain, int depth, Leaf leaf)
{
	++m_samples;

	for (int i = 0; i < m_used; ++i)
	{
		if (!SameChain(m_sites[i], chain, depth, leaf))
			continue;

		++m_sites[i].samples;
		return;
	}

	if (m_used >= kMostSites)
	{
		++m_dropped;
		return;
	}

	Site& site = m_sites[m_used++];
	std::copy(chain, chain + depth, site.chain);
	site.depth = depth;
	site.leaf = leaf;
	site.samples = 1;
}

void StallProfile::Table::Clear()
{
	m_used = 0;
	m_samples = 0;
	m_dropped = 0;
}

uint32_t StallProfile::Table::Samples() const
{
	return m_samples;
}

int StallProfile::Table::Used() const
{
	return m_used;
}

uint32_t StallProfile::Table::Dropped() const
{
	return m_dropped;
}

std::vector<StallProfile::Site> StallProfile::Table::Top(int count) const
{
	std::vector<Site> sites(m_sites, m_sites + m_used);

	std::stable_sort(sites.begin(), sites.end(), [](const Site& one, const Site& other)
	{
		return one.samples > other.samples;
	});

	if (static_cast<int>(sites.size()) > count)
		sites.resize(count);

	return sites;
}

std::string StallProfile::Describe(const Table& table, int count)
{
	if (table.Samples() == 0)
		return std::string();

	std::string text = "stall samples " + std::to_string(table.Samples()) + ":";
	const std::vector<Site> sites = table.Top(count);

	for (size_t i = 0; i < sites.size(); ++i)
		text += (i == 0 ? " " : ", ") + SiteText(sites[i]);

	return text;
}
