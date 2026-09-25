#pragma once

#include <cstdint>

struct GuideRow
{
	uint32_t colour;
	const char* name;
	const char* summary;
	const char* detail;
};

struct GuidePage
{
	const char* title;
	const char* heading;
	const GuideRow* rows;
	int rowCount;
};

struct GuideContent
{
	const char* title;
	const GuidePage* pages;
	int pageCount;
};

constexpr uint32_t kGuideNoSwatch = 0;

inline uint32_t GuideColourFromImGui(uint32_t abgr)
{
	return (abgr & 0xFF00FF00u) | ((abgr & 0x00FF0000u) >> 16) | ((abgr & 0x000000FFu) << 16);
}
