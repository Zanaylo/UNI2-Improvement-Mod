#include "Network/RoomWatch/InputStore.h"

namespace {

constexpr int32_t kEmpty = -1;

}

InputStore::InputStore(int depth)
	: m_slots(static_cast<size_t>(depth > 0 ? depth : 1))
{
	Reset();
}

bool InputStore::Put(const Slot& slot)
{
	if (!Accepts(slot.frame))
		return false;

	m_slots[Index(slot.frame)] = slot;

	while (Has(m_contiguous + 1))
		++m_contiguous;

	return true;
}

bool InputStore::Read(int frame, Slot& out) const
{
	if (!Has(frame))
		return false;

	out = m_slots[Index(frame)];
	return true;
}

void InputStore::Reset()
{
	for (Slot& slot : m_slots)
		slot.frame = kEmpty;

	m_contiguous = -1;
}

void InputStore::StartAt(int frame)
{
	Reset();
	m_contiguous = frame > 0 ? frame - 1 : -1;
}

bool InputStore::Has(int frame) const
{
	return frame >= 0 && m_slots[Index(frame)].frame == frame;
}

int InputStore::Contiguous() const
{
	return m_contiguous;
}

int InputStore::ReadyFrom(int next) const
{
	return m_contiguous >= next ? m_contiguous - next + 1 : 0;
}

bool InputStore::Accepts(int frame) const
{
	const int depth = static_cast<int>(m_slots.size());

	return frame > m_contiguous && frame - m_contiguous <= depth;
}

size_t InputStore::Index(int frame) const
{
	return static_cast<size_t>(frame) % m_slots.size();
}
