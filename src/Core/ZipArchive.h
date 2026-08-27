#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ZipArchive
{
	struct Entry
	{
		std::string name;
		std::vector<uint8_t> data;
	};

	class Writer
	{
	public:
		bool Open(const std::string& path);
		bool Add(const std::string& name, const uint8_t* data, size_t size, bool compress);
		bool AddFile(const std::string& name, const std::string& path, bool compress);
		bool Close();

		int Count() const { return m_count; }
		const char* StatusText() const { return m_status; }

	private:
		struct Record
		{
			std::string name;
			uint32_t crc;
			uint32_t stored;
			uint32_t plain;
			uint32_t offset;
			uint16_t method;
		};

		void* m_file = nullptr;
		std::vector<Record> m_records;
		int m_count = 0;
		char m_status[192] = {};
	};

	bool List(const std::string& path, std::vector<std::string>& outNames);
	bool Extract(const std::string& path, const std::string& intoFolder, int& outFiles,
		char* status, int statusSize);
}
