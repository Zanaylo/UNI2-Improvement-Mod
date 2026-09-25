#pragma once

#include <atomic>

template <typename T, T Empty>
class RequestSlot
{
public:
	void Post(T value)
	{
		m_value.store(value);
	}

	bool Take(T& out)
	{
		const T value = m_value.exchange(Empty);

		if (value == Empty)
			return false;

		out = value;
		return true;
	}

	bool IsPending() const
	{
		return m_value.load() != Empty;
	}

	void Clear()
	{
		m_value.store(Empty);
	}

private:
	std::atomic<T> m_value{ Empty };
};
