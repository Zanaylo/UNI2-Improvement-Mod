#include "Game/Stages/Bbtag/EvbWriter.h"

namespace {

constexpr uint32_t kVersion = 0x1001;
constexpr size_t kBlocks = 0x30;
constexpr size_t kStride = 0x20;
constexpr size_t kOperands = 7;
constexpr uint32_t kUnused = 0xffffffff;
constexpr size_t kCountsAt = 0x20;

void Dword(std::vector<uint8_t>& out, uint32_t value)
{
	const uint8_t bytes[4] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24) };
	out.insert(out.end(), bytes, bytes + 4);
}

void Word(std::vector<uint8_t>& out, uint16_t value)
{
	out.push_back(static_cast<uint8_t>(value));
	out.push_back(static_cast<uint8_t>(value >> 8));
}

void Names(std::vector<uint8_t>& out, const std::vector<std::string>& names)
{
	for (const std::string& name : names)
	{
		const size_t start = out.size();

		for (size_t i = 0; i < name.size() && i + 1 < kStride; ++i)
			out.push_back(static_cast<uint8_t>(name[i]));

		while (out.size() < start + kStride)
			out.push_back(0);
	}
}

void Put(std::vector<uint8_t>& out, uint32_t code, const std::vector<int32_t>& operands)
{
	Dword(out, code);

	for (size_t i = 0; i < kOperands; ++i)
		Dword(out, i < operands.size() ? static_cast<uint32_t>(operands[i]) : kUnused);
}

}

void EvbWriter::Build(const Script& script, std::vector<uint8_t>& out)
{
	out.clear();

	const size_t commands = kBlocks + (script.sheets.size() + script.names.size()) * kStride;
	const size_t last = commands + script.records.size() * kStride;

	out.insert(out.end(), { 'E', 'V', 'T', '0' });
	Dword(out, kVersion);
	Dword(out, static_cast<uint32_t>(kBlocks));
	Dword(out, static_cast<uint32_t>(last));
	Dword(out, static_cast<uint32_t>(commands));
	Dword(out, static_cast<uint32_t>(kStride));

	while (out.size() < kCountsAt)
		out.push_back(0);

	Word(out, static_cast<uint16_t>(script.sheets.size()));
	Word(out, static_cast<uint16_t>(script.names.size()));

	while (out.size() < kBlocks)
		out.push_back(0);

	Names(out, script.sheets);
	Names(out, script.names);

	for (const Record& record : script.records)
		Put(out, record.code, record.operands);

	Put(out, kUnused, {});
}
