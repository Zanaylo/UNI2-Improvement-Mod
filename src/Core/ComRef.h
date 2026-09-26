#pragma once

template <typename T>
class ComRef
{
public:
	ComRef() = default;
	ComRef(const ComRef&) = delete;
	ComRef& operator=(const ComRef&) = delete;

	~ComRef()
	{
		if (m_object != nullptr)
			m_object->Release();
	}

	T** Out()
	{
		return &m_object;
	}

	void** OutVoid()
	{
		return reinterpret_cast<void**>(&m_object);
	}

	T* Get() const
	{
		return m_object;
	}

	T* operator->() const
	{
		return m_object;
	}

	explicit operator bool() const
	{
		return m_object != nullptr;
	}

private:
	T* m_object = nullptr;
};
