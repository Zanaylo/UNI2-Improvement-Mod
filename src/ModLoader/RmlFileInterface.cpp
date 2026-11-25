#include "ModLoader/RmlFileInterface.h"

#include "Core/FileSystem.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace {

std::string NormalizePath(const std::string& raw)
{
	std::string out = raw;

	for (char& c : out)
	{
		if (c == '\\')
			c = '/';
	}

	while (out.size() >= 2 && out[0] == '.' && out[1] == '/')
		out.erase(0, 2);

	while (!out.empty() && out[0] == '/')
		out.erase(0, 1);

	return out;
}

std::string ResourceKeyOf(const std::string& normalized)
{
	std::string key;

	for (char c : normalized)
	{
		if (c == '/' || c == '\\' || c == '-' || c == '.')
			key.push_back('_');
		else
			key.push_back(static_cast<char>(toupper(static_cast<unsigned char>(c))));
	}

	return key;
}

bool LoadEmbeddedAsset(const std::string& normalized, std::vector<uint8_t>& out)
{
	const HMODULE module = GetModuleHandleA(nullptr);

	if (module == nullptr)
		return false;

	const std::string key = ResourceKeyOf(normalized);
	const HRSRC found = FindResourceA(module, key.c_str(), "RMLASSET");

	if (found == nullptr)
		return false;

	const DWORD size = SizeofResource(module, found);
	const HGLOBAL loaded = LoadResource(module, found);

	if (size == 0 || loaded == nullptr)
		return false;

	const void* const data = LockResource(loaded);

	if (data == nullptr)
		return false;

	const auto* const bytes = static_cast<const uint8_t*>(data);
	out.assign(bytes, bytes + size);
	return true;
}

}

RmlFileInterface::RmlFileInterface(const std::string& diskRoot)
	: m_diskRoot(diskRoot)
{
}

RmlFileInterface::~RmlFileInterface() = default;

Rml::FileHandle RmlFileInterface::Open(const Rml::String& path)
{
	const std::string normalized = NormalizePath(path);
	std::vector<uint8_t> data;

	if (!m_diskRoot.empty())
	{
		std::string diskPath = m_diskRoot + "\\" + normalized;

		for (char& c : diskPath)
		{
			if (c == '/')
				c = '\\';
		}

		ReadWholeFile(diskPath, data);
	}

	if (data.empty() && !LoadEmbeddedAsset(normalized, data))
		return 0;

	const uintptr_t handle = m_nextHandle++;
	m_open.emplace(handle, Entry{ std::move(data), 0 });
	return static_cast<Rml::FileHandle>(handle);
}

void RmlFileInterface::Close(Rml::FileHandle file)
{
	m_open.erase(static_cast<uintptr_t>(file));
}

size_t RmlFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file)
{
	const auto it = m_open.find(static_cast<uintptr_t>(file));

	if (it == m_open.end() || buffer == nullptr || size == 0)
		return 0;

	Entry& entry = it->second;

	if (entry.pos >= entry.data.size())
		return 0;

	const size_t avail = entry.data.size() - entry.pos;
	const size_t take = size < avail ? size : avail;
	memcpy(buffer, entry.data.data() + entry.pos, take);
	entry.pos += take;
	return take;
}

bool RmlFileInterface::Seek(Rml::FileHandle file, long offset, int origin)
{
	const auto it = m_open.find(static_cast<uintptr_t>(file));

	if (it == m_open.end())
		return false;

	Entry& entry = it->second;
	long target = 0;

	if (origin == SEEK_SET)
		target = offset;
	else if (origin == SEEK_CUR)
		target = static_cast<long>(entry.pos) + offset;
	else if (origin == SEEK_END)
		target = static_cast<long>(entry.data.size()) + offset;
	else
		return false;

	if (target < 0 || static_cast<size_t>(target) > entry.data.size())
		return false;

	entry.pos = static_cast<size_t>(target);
	return true;
}

size_t RmlFileInterface::Tell(Rml::FileHandle file)
{
	const auto it = m_open.find(static_cast<uintptr_t>(file));
	return it == m_open.end() ? 0 : it->second.pos;
}

size_t RmlFileInterface::Length(Rml::FileHandle file)
{
	const auto it = m_open.find(static_cast<uintptr_t>(file));
	return it == m_open.end() ? 0 : it->second.data.size();
}
