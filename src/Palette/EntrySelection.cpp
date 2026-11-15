#include "Palette/EntrySelection.h"

#include <cstring>

namespace {

bool IsEntry(int entry)
{
	return entry >= 0 && entry < EntrySelection::kEntries;
}

int PositionOf(int entry, const unsigned char* shown, int shownCount)
{
	for (int position = 0; position < shownCount; ++position)
	{
		if (shown[position] == entry)
			return position;
	}

	return EntrySelection::kNothing;
}

}

void EntrySelection::Pick(int entry)
{
	if (!IsEntry(entry))
		return;

	Clear();
	Add(entry);

	m_focus = entry;
	m_anchor = entry;
}

void EntrySelection::Toggle(int entry)
{
	if (!IsEntry(entry))
		return;

	if (!m_members[entry])
	{
		Add(entry);

		m_focus = entry;
		m_anchor = entry;
		return;
	}

	if (m_count == 1)
		return;

	Remove(entry);

	const int last = m_entries[m_count - 1];

	if (m_focus == entry)
		m_focus = last;

	if (m_anchor == entry)
		m_anchor = last;
}

void EntrySelection::ExtendTo(int entry, const unsigned char* shown, int shownCount)
{
	const int from = PositionOf(m_anchor, shown, shownCount);
	const int to = PositionOf(entry, shown, shownCount);

	if (from == kNothing || to == kNothing)
	{
		Pick(entry);
		return;
	}

	const int anchor = m_anchor;
	const int step = to >= from ? 1 : -1;

	Clear();

	for (int position = from; position != to + step; position += step)
		Add(shown[position]);

	m_focus = entry;
	m_anchor = anchor;
}

void EntrySelection::Clear()
{
	memset(m_members, 0, sizeof(m_members));

	m_count = 0;
	m_focus = kNothing;
	m_anchor = kNothing;
}

bool EntrySelection::Contains(int entry) const
{
	return IsEntry(entry) && m_members[entry];
}

int EntrySelection::Count() const
{
	return m_count;
}

int EntrySelection::At(int position) const
{
	return position >= 0 && position < m_count ? m_entries[position] : kNothing;
}

int EntrySelection::Focus() const
{
	return m_focus;
}

const bool* EntrySelection::Members() const
{
	return m_members;
}

void EntrySelection::Add(int entry)
{
	m_entries[m_count++] = static_cast<unsigned char>(entry);
	m_members[entry] = true;
}

void EntrySelection::Remove(int entry)
{
	const int position = PositionOf(entry, m_entries, m_count);

	memmove(m_entries + position, m_entries + position + 1, static_cast<size_t>(m_count - position - 1));

	m_members[entry] = false;
	--m_count;
}
