#include "Game/OstUniNames.h"

#include "Core/logger.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t kScanSpan = 0x800;
constexpr size_t kStringLimit = 128;

const char* const kAnchors[] = {
	"Bgm\\sys_chrselect.ogg",
	"Bgm\\sys_main.ogg",
	"Bgm\\ed_bad.ogg",
};

struct Section
{
	uint32_t virtualAddress;
	uint32_t virtualSize;
	uint32_t rawPointer;
	uint32_t rawSize;
};

uint32_t Read32(const std::vector<uint8_t>& blob, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, blob.data() + at, sizeof(value));
	return value;
}

uint16_t Read16(const std::vector<uint8_t>& blob, size_t at)
{
	uint16_t value = 0;
	memcpy(&value, blob.data() + at, sizeof(value));
	return value;
}

class Image
{
public:
	bool Open(const std::string& path);

	bool Offset(uint32_t rva, size_t& out) const;
	bool Rva(size_t offset, uint32_t& out) const;

	bool String(uint32_t va, std::string& out) const;
	bool Dword(uint32_t rva, uint32_t& out) const;

	bool PointerTo(const char* text, uint32_t& outRva) const;

private:
	std::vector<uint8_t> m_blob;
	std::vector<Section> m_sections;
	uint32_t m_base = 0;
};

bool Image::Open(const std::string& path)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	fseek(handle, 0, SEEK_END);
	const long size = ftell(handle);
	fseek(handle, 0, SEEK_SET);

	if (size <= 0x40)
	{
		fclose(handle);
		return false;
	}

	m_blob.resize(static_cast<size_t>(size));
	const size_t read = fread(m_blob.data(), 1, m_blob.size(), handle);
	fclose(handle);

	if (read != m_blob.size())
		return false;

	const uint32_t pe = Read32(m_blob, 0x3c);

	if (pe + 0x78 >= m_blob.size())
		return false;

	const uint16_t sections = Read16(m_blob, pe + 6);
	const uint16_t optional = Read16(m_blob, pe + 20);

	m_base = Read32(m_blob, pe + 24 + 28);

	const size_t table = pe + 24 + optional;

	if (table + static_cast<size_t>(sections) * 40 > m_blob.size())
		return false;

	for (uint16_t i = 0; i < sections; ++i)
	{
		const size_t at = table + static_cast<size_t>(i) * 40;

		Section section = {};
		section.virtualSize = Read32(m_blob, at + 8);
		section.virtualAddress = Read32(m_blob, at + 12);
		section.rawSize = Read32(m_blob, at + 16);
		section.rawPointer = Read32(m_blob, at + 20);

		m_sections.push_back(section);
	}

	return !m_sections.empty();
}

bool Image::Offset(uint32_t rva, size_t& out) const
{
	for (const Section& section : m_sections)
	{
		const uint32_t span = section.virtualSize > section.rawSize
			? section.virtualSize : section.rawSize;

		if (rva < section.virtualAddress || rva >= section.virtualAddress + span)
			continue;

		const size_t at = section.rawPointer + (rva - section.virtualAddress);

		if (at >= static_cast<size_t>(section.rawPointer) + section.rawSize || at >= m_blob.size())
			return false;

		out = at;
		return true;
	}

	return false;
}

bool Image::Rva(size_t offset, uint32_t& out) const
{
	for (const Section& section : m_sections)
	{
		if (offset < section.rawPointer ||
			offset >= static_cast<size_t>(section.rawPointer) + section.rawSize)
		{
			continue;
		}

		out = section.virtualAddress + static_cast<uint32_t>(offset - section.rawPointer);
		return true;
	}

	return false;
}

bool Image::String(uint32_t va, std::string& out) const
{
	out.clear();

	if (va < m_base)
		return false;

	size_t at = 0;

	if (!Offset(va - m_base, at))
		return false;

	while (at + out.size() < m_blob.size() && out.size() < kStringLimit)
	{
		const uint8_t c = m_blob[at + out.size()];

		if (c == 0)
			break;

		if (c < 0x20 || c >= 0x7f)
			return false;

		out.push_back(static_cast<char>(c));
	}

	return !out.empty();
}

bool Image::Dword(uint32_t rva, uint32_t& out) const
{
	size_t at = 0;

	if (!Offset(rva, at) || at + 4 > m_blob.size())
		return false;

	out = Read32(m_blob, at);
	return true;
}

bool Image::PointerTo(const char* text, uint32_t& outRva) const
{
	const size_t length = strlen(text) + 1;

	if (length > m_blob.size())
		return false;

	size_t found = m_blob.size();

	for (size_t i = 0; i + length <= m_blob.size(); ++i)
	{
		if (memcmp(m_blob.data() + i, text, length) != 0)
			continue;

		found = i;
		break;
	}

	if (found >= m_blob.size())
		return false;

	uint32_t rva = 0;

	if (!Rva(found, rva))
		return false;

	const uint32_t va = rva + m_base;

	int hits = 0;

	for (size_t i = 0; i + 4 <= m_blob.size(); ++i)
	{
		if (Read32(m_blob, i) != va)
			continue;

		uint32_t candidate = 0;

		if (!Rva(i, candidate))
			continue;

		if (++hits > 1)
			return false;

		outRva = candidate;
	}

	return hits == 1;
}

bool LooksLikePath(const std::string& text)
{
	return text.find('.') != std::string::npos ||
		text.find('/') != std::string::npos ||
		text.find('\\') != std::string::npos;
}

struct Slot
{
	uint32_t rva;
	std::string text;
};

typedef std::vector<Slot> Run;

std::vector<Slot> Scan(const Image& image, uint32_t centre)
{
	std::vector<Slot> found;

	const uint32_t first = centre > kScanSpan ? centre - kScanSpan : 0;

	for (uint32_t rva = first; rva < centre + kScanSpan; rva += 4)
	{
		uint32_t value = 0;

		if (!image.Dword(rva, value) || value == 0)
			continue;

		std::string text;

		if (!image.String(value, text))
			continue;

		Slot slot = { rva, text };
		found.push_back(slot);
	}

	return found;
}

std::vector<Run> Runs(const std::vector<Slot>& found, bool wantPaths)
{
	std::vector<Run> out;
	Run current;

	for (const Slot& slot : found)
	{
		const bool keep = LooksLikePath(slot.text) == wantPaths;

		if (keep && (current.empty() || slot.rva == current.back().rva + 4))
		{
			current.push_back(slot);
			continue;
		}

		if (!current.empty())
			out.push_back(current);

		current.clear();

		if (keep)
			current.push_back(slot);
	}

	if (!current.empty())
		out.push_back(current);

	return out;
}

uint32_t Distance(const Run& run, const Run& paths)
{
	const uint32_t after = run.front().rva > paths.back().rva
		? run.front().rva - paths.back().rva : paths.back().rva - run.front().rva;

	const uint32_t before = paths.front().rva > run.back().rva
		? paths.front().rva - run.back().rva : run.back().rva - paths.front().rva;

	return after < before ? after : before;
}

bool Pair(const Image& image, const char* anchor, std::map<std::string, std::string>& out)
{
	uint32_t seed = 0;

	if (!image.PointerTo(anchor, seed))
		return false;

	const std::vector<Slot> found = Scan(image, seed);

	const Run* paths = nullptr;
	const std::vector<Run> pathRuns = Runs(found, true);

	for (const Run& run : pathRuns)
	{
		for (const Slot& slot : run)
		{
			if (slot.rva != seed)
				continue;

			paths = &run;
			break;
		}

		if (paths != nullptr)
			break;
	}

	if (paths == nullptr)
		return false;

	const size_t count = paths->size();
	const std::vector<Run> nameRuns = Runs(found, false);
	const Run* best = nullptr;

	for (const Run& run : nameRuns)
	{
		if (run.size() < count)
			continue;

		if (best == nullptr || Distance(run, *paths) < Distance(*best, *paths))
			best = &run;
	}

	if (best == nullptr)
		return false;

	const size_t start = best->back().rva < paths->front().rva ? best->size() - count : 0;

	for (size_t i = 0; i < count; ++i)
		out[(*paths)[i].text] = (*best)[start + i].text;

	return true;
}

}

bool OstUniNames::Build(const std::string& exePath, std::map<std::string, std::string>& out)
{
	out.clear();

	Image image;

	if (!image.Open(exePath))
		return false;

	for (const char* anchor : kAnchors)
	{
		if (!Pair(image, anchor, out))
			continue;

		LOG("OstUniNames: %d name(s) from %s", static_cast<int>(out.size()), anchor);
		return true;
	}

	return false;
}
