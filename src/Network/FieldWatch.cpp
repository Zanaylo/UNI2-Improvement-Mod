#include "Network/FieldWatch.h"

FieldWatch::FieldWatch(int fields)
	: m_fields(static_cast<size_t>(fields > 0 ? fields : 0), Field{})
{
}

bool FieldWatch::Update(int index, int64_t value)
{
	if (!Holds(index))
		return false;

	Field& field = m_fields[static_cast<size_t>(index)];

	if (!field.seen)
	{
		field = { true, value, value };
		return false;
	}

	if (field.current == value)
		return false;

	field.previous = field.current;
	field.current = value;
	return true;
}

void FieldWatch::Reset()
{
	for (Field& field : m_fields)
		field = {};
}

int64_t FieldWatch::Previous(int index) const
{
	return Holds(index) ? m_fields[static_cast<size_t>(index)].previous : 0;
}

int64_t FieldWatch::Current(int index) const
{
	return Holds(index) ? m_fields[static_cast<size_t>(index)].current : 0;
}

bool FieldWatch::Holds(int index) const
{
	return index >= 0 && index < static_cast<int>(m_fields.size());
}
