#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class FontPageSet
{
public:
	static constexpr size_t kHeadBytes = 65536;

	void Learn(const void* head, size_t headSize, size_t fileSize);
	bool Recognises(const void* bytes, size_t size) const;
	size_t Count() const;

	static bool IsFontFolder(const std::string& folder);
	static bool IsPageFile(const std::string& file);

private:
	bool Matches(const void* head, size_t headSize, size_t fileSize) const;

	struct Print
	{
		size_t size;
		uint64_t hash;
	};

	std::vector<Print> m_prints;
};
