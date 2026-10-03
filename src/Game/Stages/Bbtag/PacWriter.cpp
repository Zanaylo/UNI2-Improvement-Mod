#include "Game/Stages/Bbtag/PacWriter.h"

#include "Core/Formats/Deflate.h"

namespace {

constexpr size_t kHeader = 0x20;
constexpr uint32_t kFlags = 1;
constexpr size_t kEntryTail = 12;
constexpr size_t kStrideSlack = 16;

size_t Align(size_t value, size_t to)
{
	return (value + to - 1) / to * to;
}

void Dword(std::vector<uint8_t>& out, uint32_t value)
{
	const uint8_t bytes[4] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24) };
	out.insert(out.end(), bytes, bytes + 4);
}

void Pad(std::vector<uint8_t>& out, size_t until)
{
	while (out.size() < until)
		out.push_back(0);
}

}

void PacWriter::Build(const std::vector<Entry>& entries, std::vector<uint8_t>& out)
{
	out.clear();

	size_t longest = 0;

	for (const Entry& entry : entries)
		longest = entry.name.size() > longest ? entry.name.size() : longest;

	const size_t nameBytes = Align(longest + 1, 4);
	const size_t stride = Align(nameBytes + kStrideSlack, 16);
	const size_t dataStart = kHeader + entries.size() * stride;

	size_t total = dataStart;

	for (const Entry& entry : entries)
		total += Align(entry.data.size(), 16);

	out.insert(out.end(), { 'F', 'P', 'A', 'C' });
	Dword(out, static_cast<uint32_t>(dataStart));
	Dword(out, static_cast<uint32_t>(total));
	Dword(out, static_cast<uint32_t>(entries.size()));
	Dword(out, kFlags);
	Dword(out, static_cast<uint32_t>(nameBytes));
	Pad(out, kHeader);

	size_t offset = 0;

	for (size_t i = 0; i < entries.size(); ++i)
	{
		const size_t start = out.size();
		out.insert(out.end(), entries[i].name.begin(), entries[i].name.end());
		Pad(out, start + nameBytes);
		Dword(out, static_cast<uint32_t>(i));
		Dword(out, static_cast<uint32_t>(offset));
		Dword(out, static_cast<uint32_t>(entries[i].data.size()));
		Pad(out, start + stride);
		offset += Align(entries[i].data.size(), 16);
	}

	for (const Entry& entry : entries)
	{
		out.insert(out.end(), entry.data.begin(), entry.data.end());
		Pad(out, Align(out.size(), 16));
	}
}

bool PacWriter::Packed(const std::vector<uint8_t>& archive, std::vector<uint8_t>& out)
{
	out.clear();

	std::vector<uint8_t> stream;

	if (archive.size() < 4 || !Deflate::Zlib(archive.data(), archive.size(), stream))
		return false;

	out.insert(out.end(), { 'D', 'F', 'A', 'S' });
	out.insert(out.end(), archive.begin(), archive.begin() + 4);
	Dword(out, static_cast<uint32_t>(archive.size()));
	Dword(out, static_cast<uint32_t>(stream.size()));
	out.insert(out.end(), stream.begin(), stream.end());

	return true;
}
