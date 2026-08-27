#include "Game/OstPac.h"

#include "Core/logger.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr size_t kHeader = 0x34;
constexpr size_t kFolderName = 0x100;
constexpr size_t kFileName = 0x20;

void Crypt(uint8_t* data, size_t size, uint32_t seed, uint8_t step, size_t skip)
{
	uint8_t key[4];
	memcpy(key, &seed, 4);

	int index = 0;

	for (size_t i = 0; i < skip; ++i)
	{
		key[index] = static_cast<uint8_t>(key[index] + step);
		index = (index + 1) & 3;
	}

	for (size_t i = 0; i < size; ++i)
	{
		data[i] ^= key[index];
		key[index] = static_cast<uint8_t>(key[index] + step);
		index = (index + 1) & 3;
	}
}

std::string TakeName(std::vector<uint8_t>& raw, uint32_t seed, uint8_t step)
{
	Crypt(raw.data(), raw.size(), seed, step != 0 ? step : 1, 0);

	size_t length = 0;

	while (length < raw.size() && raw[length] != 0)
		++length;

	return std::string(reinterpret_cast<const char*>(raw.data()), length);
}

uint32_t Read32(const uint8_t* at)
{
	uint32_t value = 0;
	memcpy(&value, at, 4);
	return value;
}

}

bool OstPac::Archive::Open(const std::string& path)
{
	m_files.clear();

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	uint8_t header[kHeader] = {};

	if (fread(header, 1, kHeader, handle) != kHeader ||
		memcmp(header, "FilePacHeaderA", 14) != 0)
	{
		fclose(handle);
		return false;
	}

	m_path = path;
	m_seed = Read32(header + 0x14);
	m_tableSize = Read32(header + 0x18);
	const uint32_t folders = Read32(header + 0x20);
	const uint32_t files = Read32(header + 0x24);
	m_crypted = Read32(header + 0x28) != 0;
	m_step = static_cast<uint8_t>(Read32(header + 0x2c));
	m_block = Read32(header + 0x30);

	if (m_step == 0)
		m_step = 1;

	if (m_tableSize <= kHeader || folders > 4096 || files > 200000)
	{
		fclose(handle);
		return false;
	}

	std::vector<uint8_t> table(m_tableSize - kHeader);

	if (fread(table.data(), 1, table.size(), handle) != table.size())
	{
		fclose(handle);
		return false;
	}

	fclose(handle);

	struct Folder
	{
		std::string name;
		uint32_t first;
	};

	std::vector<Folder> folderList;
	size_t at = 0;

	for (uint32_t i = 0; i < folders; ++i)
	{
		if (at + 12 + kFolderName > table.size())
			return false;

		const uint32_t spare = Read32(table.data() + at + 4);
		const uint32_t size = Read32(table.data() + at + 8);
		at += 12;

		std::vector<uint8_t> raw(table.begin() + at, table.begin() + at + kFolderName);
		at += kFolderName;

		Folder folder;
		folder.name = TakeName(raw, m_seed, static_cast<uint8_t>(size & 0xff));
		folder.first = spare;

		while (!folder.name.empty() && (folder.name[0] == '.' || folder.name[0] == '\\'))
			folder.name.erase(0, 1);

		folderList.push_back(folder);
	}

	for (uint32_t i = 0; i < files; ++i)
	{
		if (at + 12 + kFileName > table.size())
			return false;

		Entry entry = {};
		entry.offset = Read32(table.data() + at);
		entry.size = Read32(table.data() + at + 8);
		at += 12;

		std::vector<uint8_t> raw(table.begin() + at, table.begin() + at + kFileName);
		at += kFileName;

		entry.name = TakeName(raw, m_seed, static_cast<uint8_t>(entry.size & 0xff));
		m_files.push_back(entry);
	}

	for (size_t i = 0; i < folderList.size(); ++i)
	{
		const uint32_t first = folderList[i].first;
		const uint32_t last = i + 1 < folderList.size()
			? folderList[i + 1].first : static_cast<uint32_t>(m_files.size());

		for (uint32_t f = first; f < last && f < m_files.size(); ++f)
			m_files[f].folder = folderList[i].name;
	}

	return true;
}

bool OstPac::Archive::Read(const Entry& entry, std::vector<uint8_t>& out) const
{
	out.clear();

	if (entry.size == 0)
		return false;

	FILE* handle = nullptr;

	if (fopen_s(&handle, m_path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	if (fseek(handle, static_cast<long>(m_tableSize + entry.offset), SEEK_SET) != 0)
	{
		fclose(handle);
		return false;
	}

	out.resize(entry.size);
	const size_t read = fread(out.data(), 1, out.size(), handle);
	fclose(handle);

	if (read != out.size())
	{
		out.clear();
		return false;
	}

	if (!m_crypted)
		return true;

	const size_t head = out.size() < m_block ? out.size() : m_block;
	Crypt(out.data(), head, m_seed, m_step, 0);

	if (out.size() > static_cast<size_t>(m_block) * 2)
	{
		const size_t tail = out.size() - m_block;
		Crypt(out.data() + tail, m_block, m_seed, m_step, m_block);
	}

	return true;
}
