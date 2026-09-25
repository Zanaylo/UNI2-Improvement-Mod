#pragma once

#include <cstdint>
#include <string>
#include <vector>

class GameArchive
{
public:
	bool Open(const std::string& folder);
	void Close();

	bool IsOpen() const;

	void Folders(std::vector<std::string>& out) const;

	bool List(const char* folder, std::vector<std::string>& out) const;
	bool Read(const char* folder, const char* file, std::vector<uint8_t>& out) const;

	int FileCount() const;

private:
	struct Entry
	{
		std::string name;
		uint32_t offset;
		uint32_t size;
		int archive;
	};

	struct Folder
	{
		std::string name;
		std::vector<Entry> files;
	};

	bool TakeIndex(const std::vector<uint8_t>& data);
	const Entry* Find(const char* folder, const char* file, std::string& outArchive) const;
	const Folder* FolderFor(const char* folder) const;

	std::string m_folder;
	std::vector<std::string> m_archives;
	std::vector<Folder> m_folders;
};
