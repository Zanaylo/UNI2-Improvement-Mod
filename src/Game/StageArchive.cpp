#include "Game/StageArchive.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/FbGameFolder.h"
#include "Game/MbtlCipher.h"

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

}

StageArchive::Source* StageArchive::Open(const char* folder)
{
	if (folder == nullptr || folder[0] == 0)
		return nullptr;

	const FbGameFolder::Game game = FbGameFolder::Detect(folder);

	if (game == FbGameFolder::Game_MBTL)
	{
		MbtlSource* const source = new MbtlSource(folder);

		if (source->IsOpen())
			return source;

		delete source;
		return nullptr;
	}

	if (game == FbGameFolder::Game_UNI)
	{
		UniSource* const source = new UniSource(folder);

		if (source->IsOpen())
			return source;

		delete source;
		return nullptr;
	}

	return nullptr;
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
	const int number = NumberOf(stage);

	if (number < 0)
		return false;

	for (size_t at = bgList.find("Bg_"); at != std::string::npos; at = bgList.find("Bg_", at + 3))
	{
		size_t digits = at + 3;

		while (digits < bgList.size() && isdigit(static_cast<unsigned char>(bgList[digits])) != 0)
			++digits;

		if (digits == at + 3 || atoi(bgList.c_str() + at + 3) != number)
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

		out = bgList.substr(open + 1, end - open - 2);
		return true;
	}

	return false;
}

bool StageArchive::Field(const std::string& block, const char* key, std::string& out)
{
	const size_t length = strlen(key);

	for (size_t at = 0; at + length < block.size(); ++at)
	{
		if (!KeyAt(block, at, key, length))
			continue;

		const size_t equals = Skip(block, at + length, " \t");

		if (equals >= block.size() || block[equals] != '=')
			continue;

		const size_t value = ValueStart(block, equals + 1);
		const size_t end = ValueEnd(block, value);

		if (end == std::string::npos)
			return false;

		out = block.substr(value, end - value);

		while (!out.empty() && (out.back() == ' ' || out.back() == '\t'))
			out.pop_back();

		return !out.empty();
	}

	return false;
}
