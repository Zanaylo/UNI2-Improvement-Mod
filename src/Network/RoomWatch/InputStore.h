#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class InputStore
{
public:
	static constexpr int kBodyBytes = 0x28;

#pragma pack(push, 1)
	struct Slot
	{
		int32_t frame;
		uint8_t body[kBodyBytes];
	};
#pragma pack(pop)

	explicit InputStore(int depth);

	bool Put(const Slot& slot);
	bool Read(int frame, Slot& out) const;
	void Reset();
	void StartAt(int frame);

	bool Has(int frame) const;
	int Contiguous() const;
	int ReadyFrom(int next) const;

private:
	bool Accepts(int frame) const;
	size_t Index(int frame) const;

	std::vector<Slot> m_slots;
	int m_contiguous = -1;
};
