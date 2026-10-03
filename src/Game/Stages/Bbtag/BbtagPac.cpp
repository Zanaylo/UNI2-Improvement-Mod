#include "Game/Stages/Bbtag/BbtagPac.h"

#include "Core/Formats/Deflate.h"

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

struct Layout
{
	size_t start;
	size_t count;
	size_t nameBytes;
	size_t stride;
};

bool LayoutOf(const std::vector<uint8_t>& blob, Layout& out)
{
	if (!BbtagPac::IsArchive(blob))
		return false;

	out.start = Dword(blob, 4);
	out.count = Dword(blob, 12);
	out.nameBytes = Dword(blob, 20);

	if (out.count == 0 || out.nameBytes == 0 || out.nameBytes > 256)
		return false;

	out.stride = Align(out.nameBytes + kEntryTail);

	if (out.start <= kHeader)
		return true;

	const size_t stated = (out.start - kHeader) / out.count;

	if (stated >= out.nameBytes + kEntryTail)
		out.stride = stated;

	return true;
}

bool Gather(const std::vector<uint8_t>& blob, const std::string& prefix, int depth,
	BbtagPac::Files& out)
{
	Layout layout = {};

	if (depth > 8 || !LayoutOf(blob, layout))
		return false;

	const size_t start = layout.start;
	const size_t count = layout.count;
	const size_t nameBytes = layout.nameBytes;
	const size_t stride = layout.stride;

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

std::vector<std::string> BbtagPac::Names(const std::vector<uint8_t>& blob)
{
	std::vector<std::string> out;
	std::vector<uint8_t> plain;
	const std::vector<uint8_t>& archive = Unpacked(blob, plain) ? plain : blob;

	Layout layout = {};

	if (!LayoutOf(archive, layout))
		return out;

	for (size_t i = 0; i < layout.count; ++i)
	{
		const size_t at = kHeader + i * layout.stride;

		if (at + layout.nameBytes > archive.size())
			return std::vector<std::string>();

		out.push_back(Leaf(archive, at, layout.nameBytes));
	}

	return out;
}

const std::vector<uint8_t>* BbtagPac::Ending(const Files& files, const std::string& tail)
{
	for (const std::pair<const std::string, std::vector<uint8_t> >& file : files)
	{
		if (file.first.size() < tail.size())
			continue;

		if (_stricmp(file.first.c_str() + file.first.size() - tail.size(), tail.c_str()) == 0)
			return &file.second;
	}

	return nullptr;
}

const std::vector<uint8_t>* BbtagPac::Named(const Files& files, const std::string& leaf)
{
	for (const std::pair<const std::string, std::vector<uint8_t> >& file : files)
	{
		const size_t slash = file.first.find_last_of("/\\");
		const size_t at = slash == std::string::npos ? 0 : slash + 1;

		if (_stricmp(file.first.c_str() + at, leaf.c_str()) == 0)
			return &file.second;
	}

	return nullptr;
}
