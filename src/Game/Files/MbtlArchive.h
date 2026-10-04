#pragma once

#include <cstdint>
#include <string>
#include <vector>

class MbtlArchive
{
public:
	bool Open(const std::string& folder);

	bool List(const std::string& prefix, std::vector<std::string>& out) const;
	bool Read(const std::string& name, std::vector<uint8_t>& out) const;

	const char* Problem() const { return m_problem; }

private:
	struct Root
	{
		const char* name;
		const char* archive;
		std::vector<std::string> names;
		std::vector<uint32_t> offsets;
		uint64_t size = 0;
	};

	bool TakeNames(const std::vector<uint8_t>& section);
	bool TakeTables(const std::vector<uint8_t>& section);

	const Root* RootOf(const std::string& name) const;

	std::string m_folder;
	std::vector<Root> m_roots;
	char m_problem[160] = {};
};
