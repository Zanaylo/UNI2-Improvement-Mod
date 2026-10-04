#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

class PatBytes
{
public:
	void Tag(const char* name) { Bytes(name, 4); }
	void Byte(uint8_t value) { m_blob.push_back(value); }

	void Dword(uint32_t value)
	{
		for (int shift = 0; shift < 32; shift += 8)
			m_blob.push_back(static_cast<uint8_t>(value >> shift));
	}

	void Int(int value) { Dword(static_cast<uint32_t>(value)); }

	void Word(uint16_t value)
	{
		m_blob.push_back(static_cast<uint8_t>(value));
		m_blob.push_back(static_cast<uint8_t>(value >> 8));
	}

	void Float(float value)
	{
		uint32_t raw = 0;
		memcpy(&raw, &value, 4);
		Dword(raw);
	}

	void Bytes(const void* data, size_t size)
	{
		const uint8_t* const bytes = static_cast<const uint8_t*>(data);
		m_blob.insert(m_blob.end(), bytes, bytes + size);
	}

	void Zeros(size_t size) { m_blob.insert(m_blob.end(), size, 0); }

	void Name(const char* tag, const std::string& text)
	{
		Tag(tag);
		Byte(static_cast<uint8_t>(text.size()));
		Bytes(text.data(), text.size());
	}

	void Padded(const std::string& text, size_t size)
	{
		const size_t taken = text.size() < size ? text.size() : size;
		Bytes(text.data(), taken);
		Zeros(size - taken);
	}

	std::vector<uint8_t>& Blob() { return m_blob; }

private:
	std::vector<uint8_t> m_blob;
};
