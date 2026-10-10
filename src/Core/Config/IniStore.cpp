#include "Core/Config/IniStore.h"

#include "Core/Config/IniDocument.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr DWORD kRecheckMs = 500;
constexpr DWORD kWriteDelayMs = 250;
constexpr const char* kTemporarySuffix = ".tmp";

enum OpKind
{
	Op_Set,
	Op_Remove,
	Op_RemoveSection,
};

struct Op
{
	OpKind kind;
	std::string section;
	std::string key;
	std::string value;
};

struct Stamp
{
	bool exists;
	FILETIME written;
	DWORD size;
};

struct File
{
	std::string path;
	IniDocument document;
	Stamp stamp;
	DWORD checkedAt;
	DWORD dirtySince;
	std::vector<Op> pending;
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::map<std::string, File> g_files;
HANDLE g_wake = nullptr;
INIT_ONCE g_writerOnce = INIT_ONCE_STATIC_INIT;

class Exclusive
{
public:
	Exclusive() { AcquireSRWLockExclusive(&g_lock); }
	~Exclusive() { ReleaseSRWLockExclusive(&g_lock); }

	Exclusive(const Exclusive&) = delete;
	Exclusive& operator=(const Exclusive&) = delete;
};

std::string KeyOf(const char* path)
{
	char full[MAX_PATH] = {};
	const DWORD length = GetFullPathNameA(path, MAX_PATH, full, nullptr);
	std::string key = length > 0 && length < MAX_PATH ? full : path;

	std::transform(key.begin(), key.end(), key.begin(),
		[](char letter) { return static_cast<char>(tolower(static_cast<unsigned char>(letter))); });

	return key;
}

Stamp StampOf(const std::string& path)
{
	WIN32_FILE_ATTRIBUTE_DATA data = {};

	if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
		return Stamp{ false, {}, 0 };

	return Stamp{ true, data.ftLastWriteTime, data.nFileSizeLow };
}

bool SameStamp(const Stamp& left, const Stamp& right)
{
	return left.exists == right.exists && left.size == right.size &&
		CompareFileTime(&left.written, &right.written) == 0;
}

std::string ReadBytes(const std::string& path)
{
	FILE* file = nullptr;

	if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
		return std::string();

	std::string text;
	char chunk[4096] = {};

	for (size_t read = fread(chunk, 1, sizeof(chunk), file); read > 0; read = fread(chunk, 1, sizeof(chunk), file))
		text.append(chunk, read);

	fclose(file);
	return text;
}

void Load(File& file)
{
	file.stamp = StampOf(file.path);
	file.document = IniDocument::Parse(file.stamp.exists ? ReadBytes(file.path) : std::string());
	file.checkedAt = GetTickCount();
}

void Apply(IniDocument& document, const Op& op)
{
	switch (op.kind)
	{
	case Op_Set:
		document.Set(op.section, op.key, op.value);
		break;
	case Op_Remove:
		document.Remove(op.section, op.key);
		break;
	case Op_RemoveSection:
		document.RemoveSection(op.section);
		break;
	}
}

void Revalidate(File& file)
{
	const DWORD now = GetTickCount();

	if (!file.pending.empty() || now - file.checkedAt < kRecheckMs)
		return;

	file.checkedAt = now;

	if (!SameStamp(StampOf(file.path), file.stamp))
		Load(file);
}

File& FileFor(const char* path)
{
	const std::string key = KeyOf(path);
	const std::map<std::string, File>::iterator found = g_files.find(key);

	if (found != g_files.end())
	{
		Revalidate(found->second);
		return found->second;
	}

	File& file = g_files[key];
	file.path = path;
	Load(file);

	return file;
}

DWORD CopyOut(const std::string& text, char* out, DWORD size)
{
	if (out == nullptr || size == 0)
		return 0;

	const DWORD copied = static_cast<DWORD>((std::min)(text.size(), static_cast<size_t>(size - 1)));
	memcpy(out, text.data(), copied);
	out[copied] = '\0';

	return copied;
}

bool WriteBytes(const std::string& path, const std::string& text)
{
	const std::string temporary = path + kTemporarySuffix;
	FILE* file = nullptr;

	if (fopen_s(&file, temporary.c_str(), "wb") != 0 || file == nullptr)
		return false;

	const bool written = fwrite(text.data(), 1, text.size(), file) == text.size();
	const bool closed = fclose(file) == 0;

	if (written && closed && MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
		return true;

	DeleteFileA(temporary.c_str());
	return false;
}

struct Due
{
	std::string key;
	std::string path;
	std::string text;
	size_t applied;
};

void Prepare(File& file, Due& out)
{
	if (!SameStamp(StampOf(file.path), file.stamp))
	{
		Load(file);

		for (const Op& op : file.pending)
			Apply(file.document, op);
	}

	out.path = file.path;
	out.text = file.document.Text();
	out.applied = file.pending.size();
}

void Settle(const Due& due, bool written)
{
	const std::map<std::string, File>::iterator found = g_files.find(due.key);

	if (found == g_files.end() || !written)
		return;

	File& file = found->second;
	file.pending.erase(file.pending.begin(), file.pending.begin() + static_cast<std::ptrdiff_t>(due.applied));
	file.stamp = StampOf(file.path);
	file.checkedAt = GetTickCount();
}

std::vector<Due> TakeDue(bool everything, DWORD& wait)
{
	std::vector<Due> due;
	const DWORD now = GetTickCount();
	wait = INFINITE;

	for (std::pair<const std::string, File>& entry : g_files)
	{
		File& file = entry.second;

		if (file.pending.empty())
			continue;

		const DWORD age = now - file.dirtySince;

		if (!everything && age < kWriteDelayMs)
		{
			wait = (std::min)(wait, kWriteDelayMs - age);
			continue;
		}

		Due item;
		item.key = entry.first;
		Prepare(file, item);
		due.push_back(item);
	}

	return due;
}

void WriteDue(const std::vector<Due>& due)
{
	for (const Due& item : due)
	{
		const bool written = WriteBytes(item.path, item.text);

		Exclusive lock;
		Settle(item, written);
	}
}

DWORD WINAPI WriterThread(LPVOID)
{
	DWORD wait = INFINITE;

	for (;;)
	{
		WaitForSingleObject(g_wake, wait);

		std::vector<Due> due;

		{
			Exclusive lock;
			due = TakeDue(false, wait);
		}

		WriteDue(due);

		if (!due.empty())
			wait = 0;
	}
}

BOOL CALLBACK StartWriter(PINIT_ONCE, PVOID, PVOID*)
{
	g_wake = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	const HANDLE writer = CreateThread(nullptr, 0, &WriterThread, nullptr, 0, nullptr);

	if (writer == nullptr)
		return FALSE;

	CloseHandle(writer);
	return g_wake != nullptr;
}

void Record(File& file, const Op& op)
{
	Apply(file.document, op);

	if (file.pending.empty())
		file.dirtySince = GetTickCount();

	file.pending.push_back(op);
}

}

DWORD Ini::GetString(const char* section, const char* key, const char* fallback, char* out, DWORD size,
	const char* path)
{
	if (section == nullptr || key == nullptr || path == nullptr)
		return CopyOut(std::string(), out, size);

	std::string value;

	{
		Exclusive lock;

		if (!FileFor(path).document.Find(section, key, value))
			value = fallback != nullptr ? fallback : "";
	}

	return CopyOut(value, out, size);
}

UINT Ini::GetInt(const char* section, const char* key, int fallback, const char* path)
{
	if (section == nullptr || key == nullptr || path == nullptr)
		return static_cast<UINT>(fallback);

	Exclusive lock;

	return static_cast<UINT>(FileFor(path).document.Int(section, key, fallback));
}

DWORD Ini::GetSection(const char* section, char* out, DWORD size, const char* path)
{
	if (out == nullptr || size < 2 || section == nullptr || path == nullptr)
		return 0;

	std::vector<std::string> lines;

	{
		Exclusive lock;
		lines = FileFor(path).document.Section(section);
	}

	std::string block;

	for (const std::string& line : lines)
		block.append(line.c_str(), line.size() + 1);

	if (block.size() + 1 <= size)
	{
		memcpy(out, block.data(), block.size());
		out[block.size()] = '\0';
		return static_cast<DWORD>(block.size());
	}

	const DWORD kept = size - 2;
	memcpy(out, block.data(), kept);
	out[kept] = '\0';
	out[kept + 1] = '\0';

	return kept;
}

BOOL Ini::Write(const char* section, const char* key, const char* value, const char* path)
{
	if (path == nullptr)
		return FALSE;

	if (section == nullptr)
		return TRUE;

	InitOnceExecuteOnce(&g_writerOnce, &StartWriter, nullptr, nullptr);

	const Op op = key == nullptr ? Op{ Op_RemoveSection, section, std::string(), std::string() }
		: value == nullptr ? Op{ Op_Remove, section, key, std::string() }
		: Op{ Op_Set, section, key, value };

	{
		Exclusive lock;
		Record(FileFor(path), op);
	}

	if (g_wake != nullptr)
		SetEvent(g_wake);

	return TRUE;
}

void Ini::Flush()
{
	std::vector<Due> due;
	DWORD wait = INFINITE;

	{
		Exclusive lock;
		due = TakeDue(true, wait);
	}

	WriteDue(due);
}

void Ini::FlushOnExit()
{
	if (!TryAcquireSRWLockExclusive(&g_lock))
		return;

	DWORD wait = INFINITE;
	const std::vector<Due> due = TakeDue(true, wait);

	for (const Due& item : due)
		WriteBytes(item.path, item.text);

	ReleaseSRWLockExclusive(&g_lock);
}
