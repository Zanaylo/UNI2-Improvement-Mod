#include "Core/Text/LanguageCatalog.h"

void LanguageCatalog::Add(const Table& table)
{
	if (table.entries == nullptr)
		return;

	m_texts.reserve(m_texts.size() + table.count);

	for (size_t index = 0; index < table.count; ++index)
	{
		const Entry& entry = table.entries[index];

		if (entry.english == nullptr || entry.text == nullptr || *entry.text == '\0')
			continue;

		m_texts[entry.english] = entry.text;
	}
}

const char* LanguageCatalog::Find(std::string_view english) const
{
	if (english.empty())
		return nullptr;

	const auto found = m_texts.find(english);

	return found != m_texts.end() ? found->second : nullptr;
}
