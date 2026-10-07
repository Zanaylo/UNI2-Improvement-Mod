#pragma once

class RoundEvents
{
public:
	enum class Event
	{
		None,
		KnockOut,
		RoundStart
	};

	struct Vitals
	{
		int current;
		int max;

		bool IsRead() const { return max > 0; }
		bool IsDown() const { return IsRead() && current <= 0; }
		bool IsFull() const { return IsRead() && current >= max; }
	};

	Event Update(const Vitals& first, const Vitals& second);
	void Reset();

private:
	bool m_knockedOut = false;
};
