#include "Game/BbtagPac.h"

#include "Core/Deflate.h"

#include <cstring>

namespace {

constexpr size_t kHeader = 0x20;
constexpr size_t kEntryTail = 12;
constexpr size_t kPackedHeader = 16;

uint32_t Dword(const std::vector<uint8_t>& blob, size_t at)
{
	return static_cast<uint32_t>(blob[at]) | (static_cast<uint32_t>(blob[at + 1]) << 8)
		| (static_cast<uint32_t>(blob[at + 2]) << 16) | (static_cast<uint32_t>(blob[at + 3]) << 24);
}

size_t Align(size_t value)
{
	return (value + 15) / 16 * 16;
}

std::string Leaf(const std::vector<uint8_t>& blob, size_t at, size_t length)
{
	std::string out;

	for (size_t i = 0; i < length && blob[at + i] != 0; ++i)
		out.push_back(static_cast<char>(blob[at + i]));

	return out;
}

std::string Stem(const std::string& name)
{
	const size_t dot = name.rfind('.');

	return dot == std::string::npos ? name : name.substr(0, dot);
}

bool Gather(const std::vector<uint8_t>& blob, const std::string& prefix, int depth,
	BbtagPac::Files& out)
{
	if (depth > 8 || !BbtagPac::IsArchive(blob))
		return false;

	const size_t start = Dword(blob, 4);
	const size_t count = Dword(blob, 12);
	const size_t nameBytes = Dword(blob, 20);

	if (count == 0 || nameBytes == 0 || nameBytes > 256)
		return false;

	size_t stride = Align(nameBytes + kEntryTail);

	if (start > kHeader)
	{
		const size_t stated = (start - kHeader) / count;

		if (stated >= nameBytes + kEntryTail)
			stride = stated;
	}

	for (size_t i = 0; i < count; ++i)
	{
		const size_t at = kHeader + i * stride;

		if (at + nameBytes + kEntryTail > blob.size())
			return false;

		const size_t offset = Dword(blob, at + nameBytes + 4);
		const size_t size = Dword(blob, at + nameBytes + 8);

		if (start + offset + size > blob.size())
			return false;

		const std::string name = Leaf(blob, at, nameBytes);
		std::vector<uint8_t> body(blob.begin() + start + offset,
			blob.begin() + start + offset + size);
		std::vector<uint8_t> plain;

		if (BbtagPac::Unpacked(body, plain))
			body.swap(plain);

		if (BbtagPac::IsArchive(body) && Gather(body, prefix + Stem(name) + "/", depth + 1, out))
			continue;

		out[prefix + name] = body;
	}

	return true;
}

}

bool BbtagPac::IsArchive(const std::vector<uint8_t>& blob)
{
	return blob.size() >= kHeader && memcmp(blob.data(), "FPAC", 4) == 0;
}

bool BbtagPac::Unpacked(const std::vector<uint8_t>& blob, std::vector<uint8_t>& out)
{
	if (blob.size() < kPackedHeader + 2 || memcmp(blob.data(), "DFAS", 4) != 0)
		return false;

	return Deflate::Inflate(blob.data() + kPackedHeader + 2, blob.size() - kPackedHeader - 2, out,
		Dword(blob, 8));
}

bool BbtagPac::Walk(const std::vector<uint8_t>& blob, Files& out)
{
	out.clear();

	std::vector<uint8_t> plain;

	if (Unpacked(blob, plain))
		return Gather(plain, std::string(), 0, out);

	return Gather(blob, std::string(), 0, out);
}
