#include "Game/Display/FontPageSet.h"

#include "Core/utils.h"

#include <cstring>

namespace {

constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;
constexpr const char* kFontFolder = "grpdat\\Font";
constexpr const char* kPageExtension = ".dds";
constexpr char kSeparator = '\\';

uint64_t Hash(const void* bytes, size_t size)
{
	const unsigned char* data = static_cast<const unsigned char*>(bytes);
	uint64_t hash = kFnvOffset;

	for (size_t i = 0; i < size; ++i)
		hash = (hash ^ data[i]) * kFnvPrime;

	return hash;
}

size_t HeadOf(size_t fileSize)
{
	return fileSize < FontPageSet::kHeadBytes ? fileSize : FontPageSet::kHeadBytes;
}

}

void FontPageSet::Learn(const void* head, size_t headSize, size_t fileSize)
{
	if (head == nullptr || fileSize == 0 || headSize != HeadOf(fileSize) || Matches(head, headSize, fileSize))
		return;

	m_prints.push_back({ fileSize, Hash(head, headSize) });
}

bool FontPageSet::Recognises(const void* bytes, size_t size) const
{
	return bytes != nullptr && size != 0 && Matches(bytes, HeadOf(size), size);
}

bool FontPageSet::Matches(const void* head, size_t headSize, size_t fileSize) const
{
	bool sized = false;

	for (const Print& print : m_prints)
		sized = sized || print.size == fileSize;

	if (!sized)
		return false;

	const uint64_t hash = Hash(head, headSize);

	for (const Print& print : m_prints)
	{
		if (print.size == fileSize && print.hash == hash)
			return true;
	}

	return false;
}

size_t FontPageSet::Count() const
{
	return m_prints.size();
}

bool FontPageSet::IsFontFolder(const std::string& folder)
{
	if (!EndsWithNoCase(folder, kFontFolder))
		return false;

	const size_t before = folder.size() - strlen(kFontFolder);
	return before == 0 || folder[before - 1] == kSeparator;
}

bool FontPageSet::IsPageFile(const std::string& file)
{
	return file.size() > strlen(kPageExtension) && EndsWithNoCase(file, kPageExtension);
}
