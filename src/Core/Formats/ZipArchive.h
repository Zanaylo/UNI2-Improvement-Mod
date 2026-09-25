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

	class Source
	{
	public:
		~Source();

		bool Open(const std::string& path);

		bool OpenMemory(const uint8_t* data, size_t size);

		void Close();

		int Count() const { return static_cast<int>(m_entries.size()); }
		const std::string& Name(int index) const;

		int Find(const std::string& name) const;

		bool Read(int index, std::vector<uint8_t>& out);

	private:
		struct Record
		{
			std::string name;
			uint32_t stored;
			uint32_t plain;
			uint32_t local;
			uint16_t method;
		};

		bool ReadAt(long offset, void* destination, size_t size);
		bool ReadDirectory();

		void* m_file = nullptr;
		const uint8_t* m_data = nullptr;
		long m_size = 0;
		std::vector<Record> m_entries;
	};

	class Progress
	{
	public:
		virtual ~Progress() = default;

		virtual bool OnEntry(int done, int total) = 0;
	};

	bool List(const std::string& path, std::vector<std::string>& outNames);
	bool Extract(const std::string& path, const std::string& intoFolder, int& outFiles,
		char* status, int statusSize, Progress* progress = nullptr);

	bool MakeFolders(const std::string& path);
}
