#pragma once

class EntrySelection
{
public:
	static constexpr int kEntries = 256;
	static constexpr int kNothing = -1;

	void Pick(int entry);
	void Toggle(int entry);
	void ExtendTo(int entry, const unsigned char* shown, int shownCount);
	void Clear();

	bool Contains(int entry) const;
	int Count() const;
	int At(int position) const;
	int Focus() const;
	const bool* Members() const;

private:
	void Add(int entry);
	void Remove(int entry);

	unsigned char m_entries[kEntries] = {};
	bool m_members[kEntries] = {};
	int m_count = 0;
	int m_focus = kNothing;
	int m_anchor = kNothing;
};
