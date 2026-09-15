#include "Game/StageArchive.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/FbGameFolder.h"
#include "Game/FbxExLocal.h"
#include "Game/BbtagCrypt.h"
#include "Game/BbtagStage.h"
#include "Game/FbxToFbxEx.h"
#include "Game/MbtlCipher.h"
#include "Game/UnielCipher.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>

namespace {

struct StageMbtlEntry
{
	const char* name;
	uint32_t offset;
	uint32_t size;
};

#include "Game/StageMbtlIndex.inc"

constexpr const char* kMbtlArchive = "data006.bin";
constexpr const char* kBgList = "BgList.txt";
constexpr const char* kModel = "bg.fbx.bin";
constexpr size_t kTextSlack = 8192;
constexpr size_t kScoreSpan = 2048;
constexpr uint32_t kPhaseCount = 0x400;

constexpr size_t kUniHeader = 64;
constexpr size_t kUniFolderRecord = 128;
constexpr size_t kUniFileRecord = 64;
constexpr uint32_t kUniMaxFolders = 20000;
constexpr uint32_t kUniMaxFiles = 500000;

struct Magic
{
	const char* extension;
	const char* bytes;
	size_t length;
};

const Magic kMagics[] = {
	{ "dds", "DDS \x7c\0\0\0", 8 },
	{ "bin", "fbxex\0\0\0\0\0\0\0\0\0\0\0", 16 },
	{ "pat", "PAniDataFile", 12 },
	{ "img", "\0\0\0\0\x07\0\0\0", 8 },
};

std::string Combine(const std::string& folder, const std::string& name)
{
	std::string out = folder;

	if (!out.empty() && out.back() != '\\' && out.back() != '/')
		out.push_back('\\');

	return out + name;
}

std::string Lowered(const std::string& text)
{
	std::string out = text;

	for (char& c : out)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

	return out;
}

std::string Extension(const std::string& file)
{
	const size_t dot = file.rfind('.');

	return dot == std::string::npos ? std::string() : Lowered(file.substr(dot + 1));
}

const Magic* MagicFor(const std::string& file)
{
	const std::string extension = Extension(file);

	for (const Magic& magic : kMagics)
	{
		if (extension == magic.extension)
			return &magic;
	}

	return nullptr;
}

bool IsText(const std::string& file)
{
	const std::string extension = Extension(file);

	return extension == "txt" || extension == "ini" || extension == "csv";
}

bool LeadByte(uint8_t byte)
{
	return (byte >= 0x81 && byte <= 0x9f) || (byte >= 0xe0 && byte <= 0xfc);
}

bool TextByte(uint8_t byte)
{
	return byte == '\t' || byte == '\r' || byte == '\n' ||
		(byte >= 0x20 && byte < 0x7f) || (byte >= 0x80 && byte <= 0xfc);
}

int TextScore(const std::vector<uint8_t>& data)
{
	const size_t span = data.size() < kScoreSpan ? data.size() : kScoreSpan;

	if (span == 0)
		return 0;

	size_t good = 0;

	for (size_t i = 0; i < span; ++i)
		good += TextByte(data[i]) ? 1 : 0;

	return static_cast<int>((good * 1000) / span);
}

void TrimToText(std::vector<uint8_t>& data)
{
	for (size_t i = 0; i < data.size(); ++i)
	{
		if (TextByte(data[i]))
			continue;

		data.resize(i);
		return;
	}
}

bool ReadAt(FILE* handle, uint32_t offset, size_t size, std::vector<uint8_t>& out)
{
	if (handle == nullptr || size == 0)
		return false;

	if (fseek(handle, static_cast<long>(offset), SEEK_SET) != 0)
		return false;

	out.resize(size);
	const size_t read = fread(out.data(), 1, out.size(), handle);
	out.resize(read);

	return read > 0;
}

std::string ReadName(const std::vector<uint8_t>& blob, size_t at, size_t length)
{
	const char* const text = reinterpret_cast<const char*>(blob.data() + at);
	const size_t used = strnlen(text, length);

	return std::string(text, used);
}

struct Tally
{
	uint32_t bytes;
	bool model;
};

void Count(std::map<std::string, Tally>& tallies, const std::string& folder, const char* file,
	uint32_t size)
{
	Tally& tally = tallies[folder];

	tally.bytes += size;
	tally.model = tally.model || _stricmp(file, kModel) == 0;
}

void Compose(const std::map<std::string, Tally>& tallies, const std::string& bgList,
	std::vector<StageArchive::Stage>& out)
{
	for (const std::pair<const std::string, Tally>& folder : tallies)
	{
		if (!folder.second.model)
			continue;

		StageArchive::Stage stage;
		stage.folder = folder.first;
		stage.bytes = folder.second.bytes;

		std::string block;

		if (StageArchive::Block(bgList, stage.folder, block))
			StageArchive::Field(block, "Name", stage.name);

		out.push_back(stage);
	}
}

int NumberOf(const std::string& stage)
{
	size_t at = stage.size();

	while (at > 0 && isdigit(static_cast<unsigned char>(stage[at - 1])) != 0)
		--at;

	return at == stage.size() ? -1 : atoi(stage.c_str() + at);
}

class MbtlSource : public StageArchive::Source
{
public:
	explicit MbtlSource(const std::string& folder);
	~MbtlSource() override;

	void Stages(std::vector<StageArchive::Stage>& out) override;
	void Files(const std::string& stage, std::vector<std::string>& out) override;
	bool Read(const std::string& stage, const std::string& file,
		std::vector<uint8_t>& out) override;
	bool BgList(std::string& out) override;

	bool IsOpen() const { return m_handle != nullptr; }

private:
	static std::string Key(const std::string& stage, const std::string& file);
	const StageMbtlEntry* Find(const std::string& key) const;
	bool Take(const StageMbtlEntry& entry, const std::string& file, std::vector<uint8_t>& out);

	FILE* m_handle = nullptr;
};

MbtlSource::MbtlSource(const std::string& folder)
{
	fopen_s(&m_handle, Combine(folder, kMbtlArchive).c_str(), "rb");
}

MbtlSource::~MbtlSource()
{
	if (m_handle != nullptr)
		fclose(m_handle);
}

std::string MbtlSource::Key(const std::string& stage, const std::string& file)
{
	return stage.empty() ? file : stage + "/" + file;
}

const StageMbtlEntry* MbtlSource::Find(const std::string& key) const
{
	for (const StageMbtlEntry& entry : kMbtlStageEntries)
	{
		if (_stricmp(entry.name, key.c_str()) == 0)
			return &entry;
	}

	return nullptr;
}

bool MbtlSource::Take(const StageMbtlEntry& entry, const std::string& file,
	std::vector<uint8_t>& out)
{
	const bool text = IsText(file);
	const size_t wanted = entry.size + (text ? kTextSlack : 0);

	std::vector<uint8_t> raw;

	if (!ReadAt(m_handle, entry.offset, wanted, raw))
		return false;

	const Magic* const magic = MagicFor(file);
	const uint32_t first = MbtlCipher::Phase(raw);

	if (magic == nullptr && !text)
	{
		out = raw;
		MbtlCipher::DecryptAt(out, first);
		return true;
	}

	const size_t span = magic != nullptr ? magic->length : kScoreSpan;
	const std::vector<uint8_t> head(raw.begin(),
		raw.begin() + static_cast<ptrdiff_t>(span < raw.size() ? span : raw.size()));

	int bestScore = 0;
	uint32_t bestPhase = kPhaseCount;

	for (uint32_t step = 0; step < kPhaseCount; ++step)
	{
		const uint32_t phase = (first + step) & (kPhaseCount - 1);

		std::vector<uint8_t> probe = head;
		MbtlCipher::DecryptAt(probe, phase);

		if (magic != nullptr)
		{
			if (probe.size() < magic->length ||
				memcmp(probe.data(), magic->bytes, magic->length) != 0)
			{
				continue;
			}

			bestPhase = phase;
			break;
		}

		const int score = TextScore(probe);

		if (score > bestScore)
		{
			bestScore = score;
			bestPhase = phase;
		}
	}

	if (bestPhase >= kPhaseCount)
	{
		out.clear();
		return false;
	}

	out.swap(raw);
	MbtlCipher::DecryptAt(out, bestPhase);

	if (text)
		TrimToText(out);

	return !out.empty();
}

void MbtlSource::Stages(std::vector<StageArchive::Stage>& out)
{
	std::map<std::string, Tally> tallies;

	for (const StageMbtlEntry& entry : kMbtlStageEntries)
	{
		const char* const slash = strchr(entry.name, '/');

		if (slash == nullptr)
			continue;

		Count(tallies, std::string(entry.name, slash - entry.name), slash + 1, entry.size);
	}

	std::string bgList;
	BgList(bgList);
	Compose(tallies, bgList, out);
}

void MbtlSource::Files(const std::string& stage, std::vector<std::string>& out)
{
	const std::string prefix = Lowered(stage) + "/";

	for (const StageMbtlEntry& entry : kMbtlStageEntries)
	{
		const std::string name = Lowered(entry.name);

		if (name.compare(0, prefix.size(), prefix) != 0)
			continue;

		out.push_back(std::string(entry.name + prefix.size()));
	}
}

bool MbtlSource::Read(const std::string& stage, const std::string& file, std::vector<uint8_t>& out)
{
	const StageMbtlEntry* const entry = Find(Key(stage, file));

	return entry != nullptr && Take(*entry, file, out);
}

bool MbtlSource::BgList(std::string& out)
{
	std::vector<uint8_t> blob;

	if (!Read(std::string(), kBgList, blob))
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
}

class UniSource : public StageArchive::Source
{
public:
	explicit UniSource(const std::string& folder);
	~UniSource() override;

	void Stages(std::vector<StageArchive::Stage>& out) override;
	void Files(const std::string& stage, std::vector<std::string>& out) override;
	bool Read(const std::string& stage, const std::string& file,
		std::vector<uint8_t>& out) override;
	bool BgList(std::string& out) override;

	bool IsOpen() const { return m_handle != nullptr; }

private:
	struct Entry
	{
		std::string stage;
		std::string file;
		uint32_t offset;
		uint32_t size;
	};

	bool TakeListing(const std::string& root, const std::string& name);

	std::vector<Entry> m_entries;
	FILE* m_handle = nullptr;
};

bool ReadUniIndex(const std::string& path, std::vector<uint8_t>& out, uint32_t& folders,
	uint32_t& files)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	std::vector<uint8_t> head;
	const bool header = ReadAt(handle, 0, kUniHeader, head) && head.size() == kUniHeader;

	folders = header ? ReadLittle32(head, 0) : 0;
	files = header ? ReadLittle32(head, 4) : 0;

	const bool sane = folders != 0 && folders < kUniMaxFolders && files != 0 &&
		files < kUniMaxFiles;

	const size_t expected = sane ? kUniHeader +
		static_cast<size_t>(folders) * kUniFolderRecord +
		static_cast<size_t>(files) * kUniFileRecord : 0;

	const bool whole = sane && ReadAt(handle, 0, expected, out) && out.size() == expected &&
		fgetc(handle) == EOF;

	fclose(handle);

	return whole;
}

bool FindUniRecord(const std::vector<uint8_t>& blob, uint32_t folders, uint32_t files,
	const std::string& wanted, uint32_t& offset, uint32_t& size)
{
	size_t at = kUniHeader;
	size_t taken = 0;

	for (uint32_t folder = 0; folder < folders; ++folder)
	{
		const uint32_t count = ReadLittle32(blob, at);
		std::string folderPath = ReadName(blob, at + 12, kUniFolderRecord - 16);
		at += kUniFolderRecord;

		for (char& c : folderPath)
			c = c == '/' ? '\\' : c;

		while (!folderPath.empty() && folderPath.back() == '\\')
			folderPath.pop_back();

		for (uint32_t i = 0; i < count && taken < files; ++i, ++taken)
		{
			const size_t record = kUniHeader +
				static_cast<size_t>(folders) * kUniFolderRecord + taken * kUniFileRecord;

			const std::string leaf = ReadName(blob, record + 16, kUniFileRecord - 16);

			if (leaf.empty() || Lowered(folderPath + "\\" + leaf) != wanted)
				continue;

			size = ReadLittle32(blob, record + 4);
			offset = ReadLittle32(blob, record + 12);

			if (size == 0)
				continue;

			return true;
		}
	}

	return false;
}

bool TakeAsset(const std::string& root, const std::string& name, const std::string& wanted,
	std::vector<uint8_t>& out)
{
	std::vector<uint8_t> blob;
	uint32_t folders = 0;
	uint32_t files = 0;

	if (!ReadUniIndex(Combine(root, name), blob, folders, files))
		return false;

	const std::string archive = ReadName(blob, 12, kUniHeader - 12);

	if (archive.empty())
		return false;

	uint32_t offset = 0;
	uint32_t size = 0;

	if (!FindUniRecord(blob, folders, files, wanted, offset, size))
		return false;

	FILE* data = nullptr;

	if (fopen_s(&data, Combine(root, archive).c_str(), "rb") != 0 || data == nullptr)
		return false;

	const bool got = ReadAt(data, offset, size, out);
	fclose(data);

	return got;
}

UniSource::UniSource(const std::string& folder)
{
	const std::string root = Combine(folder, "d");

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(root, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (TakeListing(root, found.cFileName))
			break;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

UniSource::~UniSource()
{
	if (m_handle != nullptr)
		fclose(m_handle);
}

bool UniSource::TakeListing(const std::string& root, const std::string& name)
{
	const std::string path = Combine(root, name);

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	std::vector<uint8_t> blob;
	const bool header = ReadAt(handle, 0, kUniHeader, blob) && blob.size() == kUniHeader;

	const uint32_t folders = header ? ReadLittle32(blob, 0) : 0;
	const uint32_t files = header ? ReadLittle32(blob, 4) : 0;

	if (folders == 0 || folders >= kUniMaxFolders || files == 0 || files >= kUniMaxFiles)
	{
		fclose(handle);
		return false;
	}

	const size_t expected = kUniHeader + static_cast<size_t>(folders) * kUniFolderRecord +
		static_cast<size_t>(files) * kUniFileRecord;

	const bool whole = ReadAt(handle, 0, expected, blob) && blob.size() == expected &&
		fgetc(handle) == EOF;

	fclose(handle);

	if (!whole)
		return false;

	const std::string archive = ReadName(blob, 12, kUniHeader - 12);

	if (archive.empty())
		return false;

	std::vector<Entry> entries;
	size_t at = kUniHeader;
	size_t taken = 0;

	for (uint32_t folder = 0; folder < folders; ++folder)
	{
		const uint32_t count = ReadLittle32(blob, at);
		std::string path = ReadName(blob, at + 12, kUniFolderRecord - 16);
		at += kUniFolderRecord;

		for (char& c : path)
			c = c == '/' ? '\\' : c;

		while (!path.empty() && path.back() == '\\')
			path.pop_back();

		const size_t separator = path.find('\\');
		const bool wanted = Lowered(path.substr(0, separator == std::string::npos
			? path.size() : separator)) == "bg";

		for (uint32_t i = 0; i < count && taken < files; ++i, ++taken)
		{
			const size_t record = kUniHeader + static_cast<size_t>(folders) * kUniFolderRecord +
				taken * kUniFileRecord;

			if (!wanted)
				continue;

			Entry entry;
			entry.stage = separator == std::string::npos ? std::string()
				: path.substr(separator + 1);
			entry.file = ReadName(blob, record + 16, kUniFileRecord - 16);
			entry.size = ReadLittle32(blob, record + 4);
			entry.offset = ReadLittle32(blob, record + 12);

			if (!entry.file.empty() && entry.size > 0)
				entries.push_back(entry);
		}
	}

	if (entries.empty())
		return false;

	fopen_s(&m_handle, Combine(root, archive).c_str(), "rb");

	if (m_handle == nullptr)
		return false;

	m_entries.swap(entries);
	LOG("StageArchive: %s lists %d bg file(s) in %s", name.c_str(),
		static_cast<int>(m_entries.size()), archive.c_str());

	return true;
}

void UniSource::Stages(std::vector<StageArchive::Stage>& out)
{
	std::map<std::string, Tally> tallies;

	for (const Entry& entry : m_entries)
	{
		if (!entry.stage.empty())
			Count(tallies, entry.stage, entry.file.c_str(), entry.size);
	}

	std::string bgList;
	BgList(bgList);
	Compose(tallies, bgList, out);
}

void UniSource::Files(const std::string& stage, std::vector<std::string>& out)
{
	for (const Entry& entry : m_entries)
	{
		if (_stricmp(entry.stage.c_str(), stage.c_str()) == 0)
			out.push_back(entry.file);
	}
}

bool UniSource::Read(const std::string& stage, const std::string& file, std::vector<uint8_t>& out)
{
	for (const Entry& entry : m_entries)
	{
		if (_stricmp(entry.stage.c_str(), stage.c_str()) != 0 ||
			_stricmp(entry.file.c_str(), file.c_str()) != 0)
		{
			continue;
		}

		return ReadAt(m_handle, entry.offset, entry.size, out);
	}

	return false;
}

bool UniSource::BgList(std::string& out)
{
	std::vector<uint8_t> blob;

	if (!Read(std::string(), kBgList, blob))
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
}

size_t Skip(const std::string& text, size_t at, const char* of)
{
	while (at < text.size() && strchr(of, text[at]) != nullptr && text[at] != 0)
		++at;

	return at;
}

size_t ValueStart(const std::string& block, size_t after)
{
	const size_t sameLine = Skip(block, after, " \t");
	const size_t anyLine = Skip(block, after, " \t\r\n");

	return anyLine < block.size() && block[anyLine] == '[' ? anyLine : sameLine;
}

size_t ValueEnd(const std::string& block, size_t value)
{
	if (value < block.size() && block[value] == '[')
		return StageArchive::MatchPair(block, value);

	size_t end = value;

	while (end < block.size() && block[end] != ',' && block[end] != '\n' && block[end] != '\r' &&
		!(block[end] == '/' && end + 1 < block.size() && block[end + 1] == '/'))
	{
		++end;
	}

	return end;
}

bool KeyAt(const std::string& text, size_t at, const char* key, size_t length)
{
	if (at + length > text.size() || text.compare(at, length, key) != 0)
		return false;

	const char before = at == 0 ? ' ' : text[at - 1];
	const char after = at + length >= text.size() ? ' ' : text[at + length];

	return isalnum(static_cast<unsigned char>(before)) == 0 && before != '_' &&
		isalnum(static_cast<unsigned char>(after)) == 0 && after != '_';
}

size_t SkipComment(const std::string& text, size_t at)
{
	if (text[at] == '"')
	{
		const size_t close = text.find_first_of("\"\n", at + 1);
		return close == std::string::npos ? text.size() : close;
	}

	if (text[at] != '/' || at + 1 >= text.size())
		return at;

	if (text[at + 1] == '/')
	{
		const size_t line = text.find('\n', at);
		return line == std::string::npos ? text.size() : line;
	}

	if (text[at + 1] != '*')
		return at;

	const size_t close = text.find("*/", at + 2);
	return close == std::string::npos ? text.size() : close + 1;
}

bool IsKeyByte(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool IsKeyStart(const std::string& text, size_t at)
{
	return IsKeyByte(text[at]) && (text[at] < '0' || text[at] > '9') &&
		(at == 0 || !IsKeyByte(text[at - 1]));
}

size_t PairStart(const std::string& block, size_t after)
{
	const size_t anyLine = Skip(block, after, " \t\r\n");

	if (anyLine < block.size() && (block[anyLine] == '[' || block[anyLine] == '{'))
		return anyLine;

	return Skip(block, after, " \t");
}

size_t PairEnd(const std::string& block, size_t value)
{
	if (value >= block.size())
		return value;

	if (block[value] == '{')
		return StageArchive::MatchPair(block, value);

	if (block[value] == '"')
	{
		const size_t close = block.find('"', value + 1);
		return close == std::string::npos ? std::string::npos : close + 1;
	}

	size_t end = ValueEnd(block, value);

	while (end != std::string::npos && end > value && (block[end - 1] == ' ' || block[end - 1] == '\t'))
		--end;

	return end;
}

class FolderSource : public StageArchive::Source
{
public:
	explicit FolderSource(const std::string& folder)
		: m_bg(folder + "\\bg")
	{
	}

	void Stages(std::vector<StageArchive::Stage>& out) override;
	void Files(const std::string& stage, std::vector<std::string>& out) override;
	bool Read(const std::string& stage, const std::string& file,
		std::vector<uint8_t>& out) override;
	bool BgList(std::string& out) override;

	bool IsOpen() const
	{
		return GetFileAttributesA((m_bg + "\\" + kBgList).c_str()) != INVALID_FILE_ATTRIBUTES;
	}

protected:
	virtual std::string Model(const std::string& stage) const = 0;

	virtual std::string Exported(const std::string& name) const { return name; }

	virtual bool Convert(std::vector<uint8_t>& data, const std::string&) const
	{
		return !data.empty();
	}

	virtual void Decode(std::vector<uint8_t>&) const {}

	virtual std::string Named(int) { return std::string(); }

	bool Whole(const std::string& path, std::vector<uint8_t>& out) const;

	std::string m_bg;
};

class DfciSource : public FolderSource
{
public:
	explicit DfciSource(const std::string& folder)
		: FolderSource(folder)
	{
	}

protected:
	std::string Model(const std::string& stage) const override;
	std::string Exported(const std::string& name) const override;
	bool Convert(std::vector<uint8_t>& data, const std::string& stage) const override;
	std::string Named(int number) override;

private:
	void LoadEnglish();

	std::map<int, std::string> m_english;
	bool m_loaded = false;
};

class UnielSource : public FolderSource
{
public:
	explicit UnielSource(const std::string& folder)
		: FolderSource(folder)
	{
	}

protected:
	std::string Model(const std::string& stage) const override;
	std::string Exported(const std::string& name) const override;
	bool Convert(std::vector<uint8_t>& data, const std::string& stage) const override;
	void Decode(std::vector<uint8_t>& data) const override;
};

void DfciSource::LoadEnglish()
{
	if (m_loaded)
		return;

	m_loaded = true;

	std::vector<uint8_t> data;

	if (!Whole(m_bg + "\\BgList_str.txt", data))
		return;

	const std::string text(reinterpret_cast<const char*>(data.data()), data.size());
	size_t at = 0;

	while (at < text.size())
	{
		size_t end = text.find('\n', at);

		if (end == std::string::npos)
			end = text.size();

		const std::string line = text.substr(at, end - at);
		at = end + 1;

		const size_t equals = line.find('=');

		if (equals == std::string::npos)
			continue;

		size_t digits = 0;

		while (digits < equals && isspace(static_cast<unsigned char>(line[digits])) != 0)
			++digits;

		size_t stop = digits;

		while (stop < equals && isdigit(static_cast<unsigned char>(line[stop])) != 0)
			++stop;

		if (stop == digits)
			continue;

		size_t value = equals + 1;

		while (value < line.size() && isspace(static_cast<unsigned char>(line[value])) != 0)
			++value;

		size_t tail = line.size();

		while (tail > value && isspace(static_cast<unsigned char>(line[tail - 1])) != 0)
			--tail;

		if (tail <= value)
			continue;

		m_english[atoi(line.c_str() + digits)] = line.substr(value, tail - value);
	}
}

std::string DfciSource::Named(int number)
{
	LoadEnglish();

	const std::map<int, std::string>::const_iterator english = m_english.find(number);

	return english == m_english.end() ? std::string() : english->second;
}

std::string DfciSource::Exported(const std::string& name) const
{
	return _stricmp(name.c_str(), "bg.fbx") == 0 ? std::string(kModel) : name;
}

bool DfciSource::Convert(std::vector<uint8_t>& data, const std::string& stage) const
{
	std::vector<uint8_t> model;
	std::string error;

	if (FbxToFbxEx::Convert(data.data(), data.size(), model, error))
	{
		data.swap(model);
		return true;
	}

	LOG("StageArchive: %s\\bg.fbx did not convert - %s", stage.c_str(), error.c_str());
	data.clear();
	return false;
}

std::string DfciSource::Model(const std::string& stage) const
{
	const char* const names[] = { "bg.fbx", "bg.FBX" };

	for (const char* name : names)
	{
		const std::string path = m_bg + "\\" + stage + "\\" + name;

		if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
			return path;
	}

	return std::string();
}

bool FolderSource::Whole(const std::string& path, std::vector<uint8_t>& out) const
{
	if (!ReadWholeFile(path, out))
		return false;

	Decode(out);
	return true;
}

void FolderSource::Stages(std::vector<StageArchive::Stage>& out)
{
	out.clear();

	std::string bgList;
	BgList(bgList);

	WIN32_FIND_DATAA found = {};
	const HANDLE handle = FindFirstFileA((m_bg + "\\bg*").c_str(), &found);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		const std::string name = found.cFileName;

		if (name == "." || name == "..")
			continue;

		const std::string model = Model(name);

		if (model.empty())
			continue;

		StageArchive::Stage stage;
		stage.folder = name;
		stage.bytes = 0;

		WIN32_FILE_ATTRIBUTE_DATA info = {};

		if (GetFileAttributesExA(model.c_str(), GetFileExInfoStandard, &info) != 0)
			stage.bytes = info.nFileSizeLow;

		stage.name = Named(NumberOf(stage.folder));

		if (stage.name.empty())
		{
			std::string block;

			if (StageArchive::Block(bgList, stage.folder, block))
				StageArchive::Field(block, "Name", stage.name);
		}

		out.push_back(stage);
	}
	while (FindNextFileA(handle, &found) != 0);

	FindClose(handle);
}

void FolderSource::Files(const std::string& stage, std::vector<std::string>& out)
{
	out.clear();

	WIN32_FIND_DATAA found = {};
	const HANDLE handle = FindFirstFileA((m_bg + "\\" + stage + "\\*").c_str(), &found);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		const std::string exported = Exported(found.cFileName);

		if (!exported.empty())
			out.push_back(exported);
	}
	while (FindNextFileA(handle, &found) != 0);

	FindClose(handle);
}

bool FolderSource::Read(const std::string& stage, const std::string& file,
	std::vector<uint8_t>& out)
{
	out.clear();

	if (_stricmp(file.c_str(), kModel) != 0)
		return Whole(m_bg + "\\" + stage + "\\" + file, out);

	const std::string model = Model(stage);

	if (model.empty())
		return false;

	if (!Whole(model, out))
		return false;

	return Convert(out, stage);
}

bool FolderSource::BgList(std::string& out)
{
	out.clear();

	std::vector<uint8_t> data;

	if (!Whole(m_bg + "\\" + kBgList, data))
		return false;

	out.assign(reinterpret_cast<const char*>(data.data()), data.size());
	return true;
}

std::string UnielSource::Model(const std::string& stage) const
{
	const std::string path = m_bg + "\\" + stage + "\\" + kModel;

	return GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES ? std::string() : path;
}

std::string UnielSource::Exported(const std::string& name) const
{
	return _stricmp(name.c_str(), "bg.fbx") == 0 ? std::string() : name;
}

bool UnielSource::Convert(std::vector<uint8_t>& data, const std::string& stage) const
{
	if (data.empty())
		return false;

	FbxExLocal::Report report = {};

	if (FbxExLocal::Apply(data, report) && report.nodes != 0)
	{
		LOG("StageArchive: %s bakes each node's world matrix - %d node(s) and %d frame(s) put "
			"back onto their parents", stage.c_str(), report.nodes, report.frames);
	}

	return true;
}

void UnielSource::Decode(std::vector<uint8_t>& data) const
{
	UnielCipher::Decrypt(data);
}

class BbtagSource : public StageArchive::Source
{
public:
	explicit BbtagSource(const std::string& folder);

	void Stages(std::vector<StageArchive::Stage>& out) override;
	void Files(const std::string& stage, std::vector<std::string>& out) override;
	bool Read(const std::string& stage, const std::string& file,
		std::vector<uint8_t>& out) override;
	bool BgList(std::string& out) override;

	bool Flow(const std::string& stage, std::vector<float>& out);
	bool Lamps(const std::string& stage, std::vector<BbtagScript::Lamp>& out) override;
	bool Fading(const std::string& stage) override;

	bool IsOpen() const { return !m_stage.empty(); }

private:
	struct Held
	{
		std::string relative;
		uint32_t bytes;
	};

	void Sweep();
	void Listed();
	void Hashed();
	bool Whole(const std::string& relative, std::vector<uint8_t>& out) const;
	const std::string* Encrypted(const std::string& relative) const;
	bool Built(const std::string& stage);

	std::string m_root;
	std::map<std::string, Held> m_stage;
	std::map<std::string, std::string> m_hashed;
	std::string m_ready;
	BbtagStage::Result m_result;
};

#include "Game/BbtagPaths.inc"

std::string StageOf(const std::string& stem)
{
	const size_t slash = stem.find_last_of('/');

	return slash == std::string::npos ? stem : stem.substr(slash + 1);
}

std::string Readable(const std::string& stage)
{
	const std::string body = stage.compare(0, 3, "bg_") == 0 ? stage.substr(3) : stage;
	std::string out;
	bool lead = true;

	for (char letter : body)
	{
		if (letter == '_')
		{
			out.push_back(' ');
			lead = true;
			continue;
		}

		if (lead && letter >= 'a' && letter <= 'z')
			out.push_back(static_cast<char>(letter - 'a' + 'A'));
		else
			out.push_back(letter);

		lead = false;
	}

	return out;
}

uint32_t BytesOf(const std::string& path)
{
	WIN32_FILE_ATTRIBUTE_DATA info = {};

	if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &info) == 0)
		return 0;

	return info.nFileSizeLow;
}

BbtagSource::BbtagSource(const std::string& folder)
	: m_root(folder)
{
	Sweep();

	if (m_stage.empty())
		Listed();
}

void BbtagSource::Sweep()
{
	const std::string bg = Combine(Combine(m_root, "data"), "bg");

	WIN32_FIND_DATAA group = {};
	const HANDLE walk = FindFirstFileA(Combine(bg, "*").c_str(), &group);

	if (walk == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((group.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		const std::string name = group.cFileName;

		if (name == "." || name == "..")
			continue;

		WIN32_FIND_DATAA found = {};
		const HANDLE inside = FindFirstFileA(Combine(Combine(bg, name), "*_vtx.pac").c_str(),
			&found);

		if (inside == INVALID_HANDLE_VALUE)
			continue;

		do
		{
			const std::string leaf = Lowered(found.cFileName);
			const std::string tail = "_vtx.pac";

			if (leaf.size() <= tail.size()
				|| leaf.compare(leaf.size() - tail.size(), tail.size(), tail) != 0)
			{
				continue;
			}

			const std::string stage = leaf.substr(0, leaf.size() - tail.size());
			Held held;
			held.relative = "data/bg/" + Lowered(name) + "/" + stage;
			held.bytes = found.nFileSizeLow
				+ BytesOf(Combine(Combine(bg, name), stage + ".pac"))
				+ BytesOf(Combine(Combine(bg, name), stage + "_img.pac"));

			m_stage[stage] = held;
		}
		while (FindNextFileA(inside, &found) != 0);

		FindClose(inside);
	}
	while (FindNextFileA(walk, &group) != 0);

	FindClose(walk);
}

void BbtagSource::Listed()
{
	Hashed();

	if (m_hashed.empty())
		return;

	for (const char* stem : kBbtagStages)
	{
		const std::string relative = std::string("data/bg/") + stem;
		const std::string* const geometry = Encrypted(relative + "_vtx.pac");

		if (geometry == nullptr)
			continue;

		Held held;
		held.relative = relative;
		held.bytes = BytesOf(*geometry);

		const std::string* const scene = Encrypted(relative + ".pac");
		const std::string* const art = Encrypted(relative + "_img.pac");

		held.bytes += scene == nullptr ? 0 : BytesOf(*scene);
		held.bytes += art == nullptr ? 0 : BytesOf(*art);

		m_stage[StageOf(stem)] = held;
	}
}

void BbtagSource::Hashed()
{
	std::vector<std::string> pending;
	pending.push_back(Combine(m_root, "data"));

	while (!pending.empty() && m_hashed.size() < 200000)
	{
		const std::string folder = pending.back();
		pending.pop_back();

		WIN32_FIND_DATAA found = {};
		const HANDLE walk = FindFirstFileA(Combine(folder, "*").c_str(), &found);

		if (walk == INVALID_HANDLE_VALUE)
			continue;

		do
		{
			const std::string name = found.cFileName;

			if (name == "." || name == "..")
				continue;

			if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				pending.push_back(Combine(folder, name));
				continue;
			}

			if (name.size() != 32)
				continue;

			m_hashed[Lowered(name)] = Combine(folder, name);
		}
		while (FindNextFileA(walk, &found) != 0);

		FindClose(walk);
	}
}

const std::string* BbtagSource::Encrypted(const std::string& relative) const
{
	const std::map<std::string, std::string>::const_iterator found =
		m_hashed.find(BbtagCrypt::NameOf(relative));

	return found == m_hashed.end() ? nullptr : &found->second;
}

bool BbtagSource::Whole(const std::string& relative, std::vector<uint8_t>& out) const
{
	out.clear();

	std::string plain = relative;

	for (char& letter : plain)
	{
		if (letter == '/')
			letter = '\\';
	}

	if (ReadWholeFile(Combine(m_root, plain), out))
		return true;

	const std::string* const hashed = Encrypted(relative);

	if (hashed == nullptr || !ReadWholeFile(*hashed, out))
		return false;

	BbtagCrypt::Decrypt(Lowered(BbtagCrypt::NameOf(relative)), out);
	return true;
}

bool BbtagSource::Built(const std::string& stage)
{
	if (m_ready == stage)
		return !m_result.model.empty();

	m_ready.clear();
	m_result.model.clear();
	m_result.images.clear();

	const std::map<std::string, Held>::const_iterator held = m_stage.find(Lowered(stage));

	if (held == m_stage.end())
		return false;

	BbtagStage::Source source;

	if (!Whole(held->second.relative + "_vtx.pac", source.geometry))
		return false;

	Whole(held->second.relative + ".pac", source.scene);
	Whole(held->second.relative + "_img.pac", source.art);

	if (!BbtagStage::Convert(source, m_result))
	{
		LOG("StageArchive: %s could not be read as a BBTAG stage", stage.c_str());
		return false;
	}

	m_ready = stage;
	return true;
}

void BbtagSource::Stages(std::vector<StageArchive::Stage>& out)
{
	out.clear();

	for (const std::pair<const std::string, Held>& held : m_stage)
	{
		StageArchive::Stage stage;
		stage.folder = held.first;
		stage.name = Readable(held.first);
		stage.bytes = held.second.bytes;

		out.push_back(stage);
	}
}

void BbtagSource::Files(const std::string& stage, std::vector<std::string>& out)
{
	out.clear();

	if (!Built(stage))
		return;

	out.push_back(kModel);

	for (const std::pair<const std::string, std::vector<uint8_t> >& image : m_result.images)
		out.push_back(image.first);
}

bool BbtagSource::Read(const std::string& stage, const std::string& file,
	std::vector<uint8_t>& out)
{
	out.clear();

	if (!Built(stage))
		return false;

	if (_stricmp(file.c_str(), kModel) == 0)
	{
		out = m_result.model;
		return true;
	}

	const BbtagStage::Images::const_iterator image = m_result.images.find(Lowered(file));

	if (image == m_result.images.end())
		return false;

	out = image->second;
	return true;
}

bool BbtagSource::Flow(const std::string& stage, std::vector<float>& out)
{
	out.clear();

	if (!Built(stage))
		return false;

	out = m_result.flow;
	return !out.empty();
}

bool BbtagSource::Lamps(const std::string& stage, std::vector<BbtagScript::Lamp>& out)
{
	out.clear();

	if (!Built(stage))
		return false;

	out = m_result.lamps;
	return !out.empty();
}

bool BbtagSource::Fading(const std::string& stage)
{
	return Built(stage) && m_result.fading;
}

bool BbtagSource::BgList(std::string& out)
{
	out.clear();

	int index = 0;

	for (const std::pair<const std::string, Held>& held : m_stage)
	{
		char header[64] = {};
		sprintf_s(header, "\tBg_%03d =\r\n\t{\r\n\t\tName = \"", index++);

		out += header;
		out += Readable(held.first);
		out += "\",\r\n\t\tDataFile = \"" + held.first + "\",\r\n\r\n";
		out += BbtagStage::Block();
		out += "\t}\r\n";
	}

	return !out.empty();
}

template <typename T>
StageArchive::Source* Opened(const char* folder)
{
	T* const source = new T(folder);

	if (source->IsOpen())
		return source;

	delete source;
	return nullptr;
}

}

StageArchive::Source* StageArchive::Open(const char* folder)
{
	if (folder == nullptr || folder[0] == 0)
		return nullptr;

	switch (FbGameFolder::Detect(folder))
	{
	case FbGameFolder::Game_MBTL:
		return Opened<MbtlSource>(folder);

	case FbGameFolder::Game_UNI:
		return Opened<UniSource>(folder);

	case FbGameFolder::Game_UNIEL:
		return Opened<UnielSource>(folder);

	case FbGameFolder::Game_DFCI:
		return Opened<DfciSource>(folder);

	case FbGameFolder::Game_BBTAG:
	case FbGameFolder::Game_BBCF:
		return Opened<BbtagSource>(folder);

	default:
		return nullptr;
	}
}

bool StageArchive::MagicOk(const std::string& file, const std::vector<uint8_t>& data)
{
	if (data.empty())
		return false;

	const Magic* const magic = MagicFor(file);

	if (magic != nullptr)
	{
		return data.size() >= magic->length &&
			memcmp(data.data(), magic->bytes, magic->length) == 0;
	}

	return !IsText(file) || TextScore(data) > 990;
}

size_t StageArchive::MatchPair(const std::string& text, size_t open)
{
	const char opener = open < text.size() ? text[open] : 0;
	const char closer = opener == '{' ? '}' : (opener == '[' ? ']' : 0);

	if (closer == 0)
		return std::string::npos;

	int depth = 0;

	for (size_t at = open; at < text.size(); ++at)
	{
		const uint8_t byte = static_cast<uint8_t>(text[at]);

		if (LeadByte(byte) && at + 1 < text.size())
		{
			++at;
			continue;
		}

		depth += byte == opener ? 1 : (byte == closer ? -1 : 0);

		if (depth == 0)
			return at + 1;
	}

	return std::string::npos;
}

bool StageArchive::Block(const std::string& bgList, const std::string& stage, std::string& out)
{
	const std::string named = "\"" + stage + "\"";
	const int number = NumberOf(stage);

	for (int pass = 0; pass < 2; ++pass)
	{
		if (pass == 1 && number < 0)
			return false;

		for (size_t at = bgList.find("Bg_"); at != std::string::npos;
			at = bgList.find("Bg_", at + 3))
		{
			size_t digits = at + 3;

			while (digits < bgList.size()
				&& isdigit(static_cast<unsigned char>(bgList[digits])) != 0)
			{
				++digits;
			}

			if (digits == at + 3)
				continue;

			if (pass == 1 && atoi(bgList.c_str() + at + 3) != number)
				continue;

			size_t open = digits;
			int equals = 0;

			while (open < bgList.size() && bgList[open] != '{')
			{
				const char c = bgList[open];

				if (c == '=')
					++equals;
				else if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
					break;

				++open;
			}

			if (open >= bgList.size() || bgList[open] != '{' || equals != 1)
				continue;

			const size_t end = MatchPair(bgList, open);

			if (end == std::string::npos)
				return false;

			const std::string body = bgList.substr(open + 1, end - open - 2);

			if (pass == 0)
			{
				std::string data;

				if (!Field(body, "DataFile", data) || data != named)
					continue;
			}

			out = body;
			return true;
		}
	}

	return false;
}

bool StageArchive::FieldSpan(const std::string& block, const char* key, size_t& valueAt, size_t& valueEnd)
{
	const size_t length = strlen(key);

	for (size_t at = 0; at + length < block.size(); ++at)
	{
		if (!KeyAt(block, at, key, length))
			continue;

		const size_t equals = Skip(block, at + length, " \t");

		if (equals >= block.size() || block[equals] != '=')
			continue;

		valueAt = ValueStart(block, equals + 1);
		valueEnd = ValueEnd(block, valueAt);

		if (valueEnd == std::string::npos)
			return false;

		while (valueEnd > valueAt && (block[valueEnd - 1] == ' ' || block[valueEnd - 1] == '\t'))
			--valueEnd;

		return valueEnd > valueAt;
	}

	return false;
}

bool StageArchive::Field(const std::string& block, const char* key, std::string& out)
{
	size_t valueAt = 0;
	size_t valueEnd = 0;

	if (!FieldSpan(block, key, valueAt, valueEnd))
		return false;

	out = block.substr(valueAt, valueEnd - valueAt);
	return true;
}

void StageArchive::Pairs(const std::string& block, std::vector<Pair>& out)
{
	out.clear();

	for (size_t at = 0; at < block.size(); ++at)
	{
		const uint8_t byte = static_cast<uint8_t>(block[at]);

		if (LeadByte(byte))
		{
			++at;
			continue;
		}

		const size_t skipped = SkipComment(block, at);

		if (skipped != at)
		{
			at = skipped;
			continue;
		}

		if (byte == '{' || byte == '[')
		{
			const size_t close = MatchPair(block, at);

			if (close == std::string::npos)
				return;

			at = close - 1;
			continue;
		}

		if (!IsKeyStart(block, at))
			continue;

		size_t end = at;

		while (end < block.size() && IsKeyByte(block[end]))
			++end;

		const size_t equals = Skip(block, end, " \t");

		if (equals >= block.size() || block[equals] != '=')
		{
			at = end - 1;
			continue;
		}

		const size_t valueAt = PairStart(block, equals + 1);
		const size_t valueEnd = PairEnd(block, valueAt);

		if (valueEnd == std::string::npos)
			return;

		if (valueEnd <= valueAt)
		{
			at = valueAt - 1;
			continue;
		}

		out.push_back({ block.substr(at, end - at), block.substr(valueAt, valueEnd - valueAt) });
		at = valueEnd - 1;
	}
}

std::string StageArchive::Unquoted(const std::string& value)
{
	if (value.empty() || value.front() != '"')
		return value;

	return value.substr(1, value.size() - (value.size() > 1 && value.back() == '"' ? 2 : 1));
}

int StageArchive::CardIndex(const std::string& bgList, const std::string& stage)
{
	int seen = 0;

	for (size_t at = bgList.find("Bg_"); at != std::string::npos; at = bgList.find("Bg_", at + 3))
	{
		size_t digits = at + 3;

		while (digits < bgList.size() && isdigit(static_cast<unsigned char>(bgList[digits])) != 0)
			++digits;

		if (digits == at + 3)
			continue;

		std::string block;

		if (!Block(bgList, "bg" + bgList.substr(at + 3, digits - at - 3), block))
			continue;

		std::string data;

		if (!Field(block, "DataFile", data))
			continue;

		while (!data.empty() && (data.front() == '"' || data.front() == ' '))
			data.erase(data.begin());

		while (!data.empty() && (data.back() == '"' || data.back() == ' '))
			data.pop_back();

		if (data.empty())
			continue;

		++seen;

		if (_stricmp(data.c_str(), stage.c_str()) == 0)
			return seen;
	}

	return -1;
}

bool StageArchive::Asset(const char* folder, const char* path, std::vector<uint8_t>& out)
{
	if (folder == nullptr || path == nullptr)
		return false;

	std::string wanted = Lowered(path);

	for (char& c : wanted)
		c = c == '/' ? '\\' : c;

	if (FbGameFolder::Detect(folder) == FbGameFolder::Game_UNIEL)
	{
		if (!ReadWholeFile(Combine(folder, wanted), out))
			return false;

		UnielCipher::Decrypt(out);
		return true;
	}

	const std::string root = Combine(folder, "d");

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(root, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	bool got = false;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		got = TakeAsset(root, found.cFileName, wanted, out);
	}
	while (!got && FindNextFileA(search, &found) != 0);

	FindClose(search);

	return got;
}
