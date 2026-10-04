#include "Game/Files/MbtlArchive.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Files/MbtlCipher.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <set>

namespace {

struct RootSpec
{
	const char* name;
	const char* archive;
};

constexpr RootSpec kRoots[] = {
	{ "data", "data008.bin" },
	{ "grpdat", "data010.bin" },
	{ "se", "data012.bin" },
};

constexpr const char* kExe = "MBTL.exe";
constexpr const char* kNamesSection = ".rdata";
constexpr size_t kSectionHeader = 40;
constexpr size_t kMinimumName = 4;
constexpr size_t kMaximumName = 260;

struct Run
{
	size_t at;
	size_t length;
	uint32_t last;
};

std::string Combine(const std::string& folder, const char* name)
{
	std::string out = folder;

	if (!out.empty() && out.back() != '\\' && out.back() != '/')
		out.push_back('\\');

	return out + name;
}

uint64_t SizeOf(const std::string& path)
{
	WIN32_FILE_ATTRIBUTE_DATA data = {};

	if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
		return 0;

	return (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

bool UpperLess(const std::string& left, const std::string& right)
{
	const size_t common = (std::min)(left.size(), right.size());

	for (size_t i = 0; i < common; ++i)
	{
		const int a = toupper(static_cast<unsigned char>(left[i]));
		const int b = toupper(static_cast<unsigned char>(right[i]));

		if (a != b)
			return a < b;
	}

	return left.size() < right.size();
}

bool SameName(const std::string& left, const std::string& right)
{
	return _stricmp(left.c_str(), right.c_str()) == 0;
}

bool Printable(char c)
{
	return c >= 0x20 && c < 0x7f;
}

bool LooksLikeFile(const std::string& name)
{
	const size_t slash = name.find_last_of('/');
	const size_t dot = name.find_last_of('.');

	return slash != std::string::npos && dot != std::string::npos && dot > slash + 1 &&
		dot + 1 < name.size() && name.find('%') == std::string::npos;
}

bool Section(const std::vector<uint8_t>& image, const char* wanted, std::vector<uint8_t>& out)
{
	if (image.size() < 0x40 || image[0] != 'M' || image[1] != 'Z')
		return false;

	const uint32_t header = ReadLittle32(image, 0x3c);

	if (header + 24 > image.size())
		return false;

	const uint16_t sections = static_cast<uint16_t>(ReadLittle32(image, header + 6));
	const uint16_t optional = static_cast<uint16_t>(ReadLittle32(image, header + 20));
	const size_t table = header + 24 + optional;

	for (uint16_t i = 0; i < sections; ++i)
	{
		const size_t entry = table + i * kSectionHeader;

		if (entry + kSectionHeader > image.size())
			return false;

		if (strncmp(reinterpret_cast<const char*>(&image[entry]), wanted, 8) != 0)
			continue;

		const uint32_t size = ReadLittle32(image, entry + 16);
		const uint32_t offset = ReadLittle32(image, entry + 20);

		if (static_cast<size_t>(offset) + size > image.size())
			return false;

		out.assign(image.begin() + offset, image.begin() + offset + size);
		return true;
	}

	return false;
}

std::vector<Run> Runs(const std::vector<uint8_t>& section)
{
	std::vector<Run> runs;
	const size_t count = section.size() / 4;
	size_t start = 0;

	for (size_t i = 1; i <= count; ++i)
	{
		const bool rising = i < count && ReadLittle32(section, i * 4) >= ReadLittle32(section, (i - 1) * 4);

		if (rising)
			continue;

		const size_t length = i - start;

		if (length > 1 && ReadLittle32(section, start * 4) == 0)
			runs.push_back(Run{ start * 4, length, ReadLittle32(section, (i - 1) * 4) });

		start = i;
	}

	return runs;
}

}

bool MbtlArchive::Open(const std::string& folder)
{
	m_folder = folder;
	m_roots.clear();
	m_problem[0] = '\0';

	std::vector<uint8_t> image;

	if (!ReadWholeFile(Combine(folder, kExe), image))
	{
		strncpy_s(m_problem, "MBTL.exe could not be read", _TRUNCATE);
		return false;
	}

	std::vector<uint8_t> section;

	if (!Section(image, kNamesSection, section))
	{
		strncpy_s(m_problem, "MBTL.exe has no file table this mod can read", _TRUNCATE);
		return false;
	}

	for (const RootSpec& spec : kRoots)
	{
		Root root;
		root.name = spec.name;
		root.archive = spec.archive;
		root.size = SizeOf(Combine(folder, spec.archive));
		m_roots.push_back(root);
	}

	if (!TakeNames(section) || !TakeTables(section))
		return false;

	for (const Root& root : m_roots)
	{
		LOG("MbtlArchive: %s holds %d name(s) and %d entr(ies)", root.archive,
			static_cast<int>(root.names.size()), static_cast<int>(root.offsets.size()));
	}

	return true;
}

bool MbtlArchive::TakeNames(const std::vector<uint8_t>& section)
{
	std::vector<std::set<std::string>> seen(m_roots.size());

	for (size_t at = 1; at < section.size(); ++at)
	{
		if (section[at - 1] != 0 || !Printable(static_cast<char>(section[at])))
			continue;

		const char* const text = reinterpret_cast<const char*>(&section[at]);
		const size_t length = strnlen(text, (std::min)(kMaximumName, section.size() - at));

		if (length < kMinimumName || at + length >= section.size())
			continue;

		const std::string name(text, length);

		if (!std::all_of(name.begin(), name.end(), Printable) || !LooksLikeFile(name))
			continue;

		for (size_t i = 0; i < m_roots.size(); ++i)
		{
			const size_t rootLength = strlen(m_roots[i].name);

			if (name.size() <= rootLength || name[rootLength] != '/' ||
				name.compare(0, rootLength, m_roots[i].name) != 0)
			{
				continue;
			}

			std::string folded = name;

			for (char& c : folded)
				c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

			if (seen[i].insert(folded).second)
				m_roots[i].names.push_back(name);
		}

		at += length;
	}

	for (Root& root : m_roots)
	{
		std::sort(root.names.begin(), root.names.end(), UpperLess);

		if (!root.names.empty())
			continue;

		sprintf_s(m_problem, "MBTL.exe names nothing inside %s", root.archive);
		return false;
	}

	return true;
}

bool MbtlArchive::TakeTables(const std::vector<uint8_t>& section)
{
	const std::vector<Run> runs = Runs(section);

	for (Root& root : m_roots)
	{
		const Run* best = nullptr;

		for (const Run& run : runs)
		{
			if (run.last >= root.size || run.length < root.names.size() / 2)
				continue;

			if (best == nullptr || run.last > best->last)
				best = &run;
		}

		if (best == nullptr)
		{
			sprintf_s(m_problem, "MBTL.exe has no table for %s", root.archive);
			return false;
		}

		root.offsets.resize(best->length);

		for (size_t i = 0; i < best->length; ++i)
			root.offsets[i] = ReadLittle32(section, best->at + i * 4);
	}

	return true;
}

const MbtlArchive::Root* MbtlArchive::RootOf(const std::string& name) const
{
	for (const Root& root : m_roots)
	{
		const size_t length = strlen(root.name);

		if (name.size() > length && name[length] == '/' && _strnicmp(name.c_str(), root.name, length) == 0)
			return &root;
	}

	return nullptr;
}

bool MbtlArchive::List(const std::string& prefix, std::vector<std::string>& out) const
{
	const Root* const root = RootOf(prefix);

	if (root == nullptr)
		return false;

	for (const std::string& name : root->names)
	{
		if (name.size() >= prefix.size() && _strnicmp(name.c_str(), prefix.c_str(), prefix.size()) == 0)
			out.push_back(name);
	}

	return true;
}

bool MbtlArchive::Read(const std::string& name, std::vector<uint8_t>& out) const
{
	out.clear();

	const Root* const root = RootOf(name);

	if (root == nullptr)
		return false;

	const auto found = std::lower_bound(root->names.begin(), root->names.end(), name, UpperLess);

	if (found == root->names.end() || !SameName(*found, name))
		return false;

	const size_t index = static_cast<size_t>(found - root->names.begin());

	if (index >= root->offsets.size())
		return false;

	const uint64_t begin = root->offsets[index];
	const uint64_t end = index + 1 < root->offsets.size() ? root->offsets[index + 1] : root->size;

	if (end <= begin || end > root->size)
		return false;

	FILE* handle = nullptr;

	if (fopen_s(&handle, Combine(m_folder, root->archive).c_str(), "rb") != 0 || handle == nullptr)
		return false;

	out.resize(static_cast<size_t>(end - begin));

	const bool read = _fseeki64(handle, static_cast<long long>(begin), SEEK_SET) == 0 &&
		fread(out.data(), 1, out.size(), handle) == out.size();

	fclose(handle);

	if (!read)
	{
		out.clear();
		return false;
	}

	MbtlCipher::Decrypt(out);
	return true;
}
