#include "Game/Audio/BgmTableSites.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr uint8_t kInt3 = 0xcc;
constexpr uint8_t kCompareEax = 0x3d;
constexpr uint8_t kCompareGroup = 0x81;
constexpr uint8_t kCompareRegister = 0xf8;
constexpr uint8_t kPushDword = 0x68;
constexpr uint8_t kPushByte = 0x6a;
constexpr size_t kFunctionWindow = 0x400;
constexpr size_t kClearLead = 8;
constexpr int kIdMargin = 2;
constexpr size_t kLeastFields = 12;
constexpr size_t kMostFields = 18;
constexpr size_t kEnds = 1;
constexpr size_t kBounds = 3;
constexpr size_t kClears = 1;

uint32_t Dword(const uint8_t* code, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, code + at, sizeof(value));

	return value;
}

uint32_t Bytes(const BgmTableSites::Table& table)
{
	return table.stride * static_cast<uint32_t>(table.slots);
}

bool IsBoundValue(uint32_t value, const BgmTableSites::Table& stock)
{
	return value == static_cast<uint32_t>(stock.slots) || value == static_cast<uint32_t>(stock.slots + kIdMargin);
}

size_t FunctionStart(const uint8_t* code, size_t at)
{
	const size_t floor = at > kFunctionWindow ? at - kFunctionWindow : 0;
	size_t start = at;

	while (start > floor + 1 && !(code[start - 1] == kInt3 && code[start - 2] == kInt3))
		--start;

	return start;
}

void AddOnce(std::vector<size_t>& sites, size_t at)
{
	if (std::find(sites.begin(), sites.end(), at) == sites.end())
		sites.push_back(at);
}

void BoundsBefore(const uint8_t* code, size_t field, const BgmTableSites::Table& stock, std::vector<size_t>& out)
{
	for (size_t at = FunctionStart(code, field); at + 6 <= field; ++at)
	{
		if (code[at] == kCompareEax && IsBoundValue(Dword(code, at + 1), stock))
			AddOnce(out, at + 1);

		if (code[at] == kCompareGroup && (code[at + 1] & kCompareRegister) == kCompareRegister &&
			IsBoundValue(Dword(code, at + 2), stock))
		{
			AddOnce(out, at + 2);
		}
	}
}

bool IsClear(const uint8_t* code, size_t field, const BgmTableSites::Table& stock)
{
	if (field < kClearLead)
		return false;

	return code[field - 1] == kPushDword && code[field - 2] == 0 && code[field - 3] == kPushByte &&
		code[field - kClearLead] == kPushDword && Dword(code, field - kClearLead + 1) == Bytes(stock);
}

}

BgmTableSites::Plan BgmTableSites::Find(const uint8_t* code, size_t size, const Table& stock)
{
	Plan plan;

	if (code == nullptr || size < sizeof(uint32_t))
		return plan;

	const uint32_t end = stock.base + Bytes(stock);

	for (size_t at = 0; at + sizeof(uint32_t) <= size; ++at)
	{
		const uint32_t value = Dword(code, at);

		if (value >= stock.base && value < stock.base + stock.stride)
			plan.fields.push_back(at);
		else if (value > end && value < end + stock.stride)
			plan.ends.push_back(at);
	}

	for (size_t field : plan.fields)
	{
		BoundsBefore(code, field, stock, plan.bounds);

		if (Dword(code, field) == stock.base && IsClear(code, field, stock))
			plan.clears.push_back(field - kClearLead + 1);
	}

	return plan;
}

bool BgmTableSites::Expected(const Plan& plan)
{
	return plan.fields.size() >= kLeastFields && plan.fields.size() <= kMostFields && plan.ends.size() == kEnds &&
		plan.bounds.size() == kBounds && plan.clears.size() == kClears;
}

uint32_t BgmTableSites::Field(uint32_t value, const Table& from, const Table& to)
{
	return to.base + (value - from.base);
}

uint32_t BgmTableSites::End(uint32_t value, const Table& from, const Table& to)
{
	return to.base + Bytes(to) + (value - (from.base + Bytes(from)));
}

uint32_t BgmTableSites::Bound(uint32_t value, const Table& from, const Table& to)
{
	return value - static_cast<uint32_t>(from.slots) + static_cast<uint32_t>(to.slots);
}

uint32_t BgmTableSites::Clear(const Table& to)
{
	return Bytes(to);
}
