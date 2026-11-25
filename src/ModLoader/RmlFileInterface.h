#pragma once

#include <RmlUi/Core/FileInterface.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class RmlFileInterface final : public Rml::FileInterface
{
public:
	explicit RmlFileInterface(const std::string& diskRoot);
	~RmlFileInterface() override;

	Rml::FileHandle Open(const Rml::String& path) override;
	void Close(Rml::FileHandle file) override;
	size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
	bool Seek(Rml::FileHandle file, long offset, int origin) override;
	size_t Tell(Rml::FileHandle file) override;
	size_t Length(Rml::FileHandle file) override;

private:
	struct Entry
	{
		std::vector<uint8_t> data;
		size_t pos = 0;
	};

	std::string m_diskRoot;
	uintptr_t m_nextHandle = 1;
	std::unordered_map<uintptr_t, Entry> m_open;
};
