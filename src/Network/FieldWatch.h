#pragma once

#include <cstdint>
#include <vector>

class FieldWatch
{
public:
	explicit FieldWatch(int fields);

	bool Update(int index, int64_t value);
	void Reset();

	int64_t Previous(int index) const;
	int64_t Current(int index) const;

private:
	struct Field
	{
		bool seen;
		int64_t previous;
		int64_t current;
	};

	bool Holds(int index) const;

	std::vector<Field> m_fields;
};
