#include "Core/ZipArchive.h"

#include "Core/Deflate.h"
#include "Core/logger.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kLocalSignature = 0x04034b50;
constexpr uint32_t kCentralSignature = 0x02014b50;
constexpr uint32_t kEndSignature = 0x06054b50;

constexpr uint16_t kStored = 0;
constexpr uint16_t kDeflated = 8;

uint32_t g_crcTable[256];
bool g_crcReady = false;

void BuildCrcTable()
{
	if (g_crcReady)
		return;

	for (uint32_t i = 0; i < 256; ++i)
	{
		uint32_t value = i;

		for (int bit = 0; bit < 8; ++bit)
			value = (value & 1) != 0 ? 0xedb88320u ^ (value >> 1) : value >> 1;

		g_crcTable[i] = value;
	}

	g_crcReady = true;
}

uint32_t Crc32(const uint8_t* data, size_t size)
{
	BuildCrcTable();

	uint32_t crc = 0xffffffffu;

	for (size_t i = 0; i < size; ++i)
		crc = g_crcTable[(crc ^ data[i]) & 0xff] ^ (crc >> 8);

	return crc ^ 0xffffffffu;
}

void Put16(std::vector<uint8_t>& out, uint16_t value)
{
	out.push_back(static_cast<uint8_t>(value & 0xff));
	out.push_back(static_cast<uint8_t>(value >> 8));
}

void Put32(std::vector<uint8_t>& out, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
		out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xff));
}

uint16_t Read16(const uint8_t* at)
{
	return static_cast<uint16_t>(at[0] | (at[1] << 8));
}

uint32_t Read32(const uint8_t* at)
{
	return static_cast<uint32_t>(at[0]) | (static_cast<uint32_t>(at[1]) << 8) |
		(static_cast<uint32_t>(at[2]) << 16) | (static_cast<uint32_t>(at[3]) << 24);
}

bool ReadWhole(const std::string& path, std::vector<uint8_t>& out)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	fseek(handle, 0, SEEK_END);
	const long size = ftell(handle);
	fseek(handle, 0, SEEK_SET);

	if (size <= 0)
	{
		fclose(handle);
		return false;
	}

	out.resize(static_cast<size_t>(size));
	const size_t read = fread(out.data(), 1, out.size(), handle);
	fclose(handle);

	return read == out.size();
}

bool MakeFolders(const std::string& path)
{
	for (size_t at = 0; at < path.size(); ++at)
	{
		if (path[at] != '\\' && path[at] != '/')
			continue;

		const std::string folder = path.substr(0, at);

		if (folder.size() > 2)
			CreateDirectoryA(folder.c_str(), nullptr);
	}

	return true;
}

bool SafeName(const std::string& name)
{
	if (name.empty() || name[0] == '/' || name[0] == '\\')
		return false;

	if (name.find("..") != std::string::npos)
		return false;

	return name.find(':') == std::string::npos;
}

}

bool ZipArchive::Writer::Open(const std::string& path)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
	{
		sprintf_s(m_status, "could not write %s", path.c_str());
		return false;
	}

	m_file = handle;
	m_records.clear();
	m_count = 0;
	strncpy_s(m_status, "writing", _TRUNCATE);
	return true;
}

bool ZipArchive::Writer::Add(const std::string& name, const uint8_t* data, size_t size,
	bool compress)
{
	if (m_file == nullptr)
		return false;

	FILE* handle = static_cast<FILE*>(m_file);

	std::vector<uint8_t> packed;
	uint16_t method = kStored;

	if (compress && size > 0 && Deflate::Compress(data, size, packed) && packed.size() < size)
		method = kDeflated;
	else
		packed.assign(data, data + size);

	Record record = {};
	record.name = name;
	record.crc = Crc32(data, size);
	record.stored = static_cast<uint32_t>(packed.size());
	record.plain = static_cast<uint32_t>(size);
	record.offset = static_cast<uint32_t>(ftell(handle));
	record.method = method;

	std::vector<uint8_t> header;
	Put32(header, kLocalSignature);
	Put16(header, 20);
	Put16(header, 0);
	Put16(header, method);
	Put16(header, 0);
	Put16(header, 0);
	Put32(header, record.crc);
	Put32(header, record.stored);
	Put32(header, record.plain);
	Put16(header, static_cast<uint16_t>(name.size()));
	Put16(header, 0);

	fwrite(header.data(), 1, header.size(), handle);
	fwrite(name.data(), 1, name.size(), handle);

	if (!packed.empty())
		fwrite(packed.data(), 1, packed.size(), handle);

	m_records.push_back(record);
	++m_count;
	return true;
}

bool ZipArchive::Writer::AddFile(const std::string& name, const std::string& path, bool compress)
{
	std::vector<uint8_t> blob;

	if (!ReadWhole(path, blob))
	{
		sprintf_s(m_status, "could not read %s", path.c_str());
		return false;
	}

	return Add(name, blob.data(), blob.size(), compress);
}

bool ZipArchive::Writer::Close()
{
	if (m_file == nullptr)
		return false;

	FILE* handle = static_cast<FILE*>(m_file);
	const uint32_t start = static_cast<uint32_t>(ftell(handle));

	for (const Record& record : m_records)
	{
		std::vector<uint8_t> entry;
		Put32(entry, kCentralSignature);
		Put16(entry, 20);
		Put16(entry, 20);
		Put16(entry, 0);
		Put16(entry, record.method);
		Put16(entry, 0);
		Put16(entry, 0);
		Put32(entry, record.crc);
		Put32(entry, record.stored);
		Put32(entry, record.plain);
		Put16(entry, static_cast<uint16_t>(record.name.size()));
		Put16(entry, 0);
		Put16(entry, 0);
		Put16(entry, 0);
		Put16(entry, 0);
		Put32(entry, 0);
		Put32(entry, record.offset);

		fwrite(entry.data(), 1, entry.size(), handle);
		fwrite(record.name.data(), 1, record.name.size(), handle);
	}

	const uint32_t size = static_cast<uint32_t>(ftell(handle)) - start;

	std::vector<uint8_t> end;
	Put32(end, kEndSignature);
	Put16(end, 0);
	Put16(end, 0);
	Put16(end, static_cast<uint16_t>(m_records.size()));
	Put16(end, static_cast<uint16_t>(m_records.size()));
	Put32(end, size);
	Put32(end, start);
	Put16(end, 0);

	fwrite(end.data(), 1, end.size(), handle);
	fclose(handle);

	m_file = nullptr;
	sprintf_s(m_status, "%d file(s) written", m_count);
	return true;
}

namespace {

bool FindCentral(const std::vector<uint8_t>& blob, uint32_t& outOffset, uint16_t& outCount)
{
	if (blob.size() < 22)
		return false;

	const size_t limit = blob.size() > 66000 ? blob.size() - 66000 : 0;

	for (size_t at = blob.size() - 22; ; --at)
	{
		if (Read32(blob.data() + at) == kEndSignature)
		{
			outCount = Read16(blob.data() + at + 10);
			outOffset = Read32(blob.data() + at + 16);
			return outOffset < blob.size();
		}

		if (at == limit)
			break;
	}

	return false;
}

}

bool ZipArchive::List(const std::string& path, std::vector<std::string>& outNames)
{
	std::vector<uint8_t> blob;

	if (!ReadWhole(path, blob))
		return false;

	uint32_t at = 0;
	uint16_t count = 0;

	if (!FindCentral(blob, at, count))
		return false;

	for (uint16_t i = 0; i < count && at + 46 <= blob.size(); ++i)
	{
		if (Read32(blob.data() + at) != kCentralSignature)
			break;

		const uint16_t nameLength = Read16(blob.data() + at + 28);
		const uint16_t extra = Read16(blob.data() + at + 30);
		const uint16_t comment = Read16(blob.data() + at + 32);

		if (at + 46 + nameLength > blob.size())
			break;

		outNames.push_back(std::string(reinterpret_cast<const char*>(blob.data() + at + 46),
			nameLength));

		at += 46 + nameLength + extra + comment;
	}

	return true;
}

bool ZipArchive::Extract(const std::string& path, const std::string& intoFolder, int& outFiles,
	char* status, int statusSize)
{
	outFiles = 0;

	std::vector<uint8_t> blob;

	if (!ReadWhole(path, blob))
	{
		strncpy_s(status, statusSize, "the file could not be read", _TRUNCATE);
		return false;
	}

	uint32_t at = 0;
	uint16_t count = 0;

	if (!FindCentral(blob, at, count))
	{
		strncpy_s(status, statusSize, "that is not a zip", _TRUNCATE);
		return false;
	}

	int refused = 0;

	for (uint16_t i = 0; i < count && at + 46 <= blob.size(); ++i)
	{
		if (Read32(blob.data() + at) != kCentralSignature)
			break;

		const uint16_t method = Read16(blob.data() + at + 10);
		const uint32_t stored = Read32(blob.data() + at + 20);
		const uint32_t plain = Read32(blob.data() + at + 24);
		const uint16_t nameLength = Read16(blob.data() + at + 28);
		const uint16_t extra = Read16(blob.data() + at + 30);
		const uint16_t comment = Read16(blob.data() + at + 32);
		const uint32_t local = Read32(blob.data() + at + 42);

		std::string name(reinterpret_cast<const char*>(blob.data() + at + 46), nameLength);
		at += 46 + nameLength + extra + comment;

		if (name.empty() || name.back() == '/' || name.back() == '\\')
			continue;

		if (!SafeName(name))
		{
			++refused;
			continue;
		}

		if (local + 30 > blob.size() || Read32(blob.data() + local) != kLocalSignature)
			continue;

		const uint16_t localName = Read16(blob.data() + local + 26);
		const uint16_t localExtra = Read16(blob.data() + local + 28);
		const size_t start = local + 30 + localName + localExtra;

		if (start + stored > blob.size())
			continue;

		std::vector<uint8_t> data;

		if (method == kStored)
		{
			data.assign(blob.begin() + start, blob.begin() + start + stored);
		}
		else if (method == kDeflated)
		{
			if (!Deflate::Inflate(blob.data() + start, stored, data, plain))
				continue;
		}
		else
		{
			continue;
		}

		std::string target = intoFolder;

		if (!target.empty() && target.back() != '\\')
			target.push_back('\\');

		for (char& c : name)
		{
			if (c == '/')
				c = '\\';
		}

		target += name;
		MakeFolders(target);

		FILE* handle = nullptr;

		if (fopen_s(&handle, target.c_str(), "wb") != 0 || handle == nullptr)
			continue;

		if (!data.empty())
			fwrite(data.data(), 1, data.size(), handle);

		fclose(handle);
		++outFiles;
	}

	if (refused > 0)
	{
		sprintf_s(status, statusSize, "%d file(s) extracted, %d refused for their path", outFiles,
			refused);
	}
	else
	{
		sprintf_s(status, statusSize, "%d file(s) extracted", outFiles);
	}

	return outFiles > 0;
}
