#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OstPac
{
	struct Entry
	{
		std::string folder;
		std::string name;
		uint32_t offset;
		uint32_t size;
	};

	class Archive
	{
	public:
		bool Open(const std::string& path);

		int Count() const { return static_cast<int>(m_files.size()); }
		const Entry& At(int index) const { return m_files[index]; }

		bool Read(const Entry& entry, std::vector<uint8_t>& out) const;

	private:
		std::string m_path;
		uint32_t m_seed = 0;
		uint32_t m_tableSize = 0;
		uint8_t m_step = 1;
		bool m_crypted = false;
		uint32_t m_block = 0x1000;
		std::vector<Entry> m_files;
	};
}
