#include "Core/Json.h"

#include <cstdlib>
#include <cstring>

namespace Json {

class Reader
{
public:
	Reader(const std::string& text, size_t depthLimit)
		: m_text(text), m_depthLimit(depthLimit)
	{
	}

	bool ReadValue(Value& out, size_t depth);
	void SkipSpace();
	bool AtEnd() const { return m_at >= m_text.size(); }

private:
	char Peek() const { return m_text[m_at]; }

	bool ReadString(std::string& out);
	bool ReadNumber(Value& out);
	bool ReadLiteral(const char* word, Value& out);
	bool ReadArray(Value& out, size_t depth);
	bool ReadObject(Value& out, size_t depth);
	bool ReadEscape(std::string& out);

	const std::string& m_text;
	size_t m_at = 0;
	size_t m_depthLimit;
};

void Reader::SkipSpace()
{
	while (m_at < m_text.size())
	{
		const char c = m_text[m_at];

		if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
			return;

		++m_at;
	}
}

bool Reader::ReadEscape(std::string& out)
{
	if (m_at >= m_text.size())
		return false;

	const char c = m_text[m_at++];

	switch (c)
	{
	case '"': out.push_back('"'); return true;
	case '\\': out.push_back('\\'); return true;
	case '/': out.push_back('/'); return true;
	case 'b': out.push_back('\b'); return true;
	case 'f': out.push_back('\f'); return true;
	case 'n': out.push_back('\n'); return true;
	case 'r': out.push_back('\r'); return true;
	case 't': out.push_back('\t'); return true;
	default: break;
	}

	if (c != 'u' || m_at + 4 > m_text.size())
		return false;

	char digits[5] = {};
	memcpy(digits, m_text.c_str() + m_at, 4);
	m_at += 4;

	const unsigned code = static_cast<unsigned>(strtoul(digits, nullptr, 16));

	if (code < 0x80)
	{
		out.push_back(static_cast<char>(code));
		return true;
	}

	if (code < 0x800)
	{
		out.push_back(static_cast<char>(0xc0 | (code >> 6)));
		out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
		return true;
	}

	out.push_back(static_cast<char>(0xe0 | (code >> 12)));
	out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
	out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
	return true;
}

bool Reader::ReadString(std::string& out)
{
	out.clear();

	if (AtEnd() || Peek() != '"')
		return false;

	++m_at;

	while (m_at < m_text.size())
	{
		const char c = m_text[m_at++];

		if (c == '"')
			return true;

		if (c != '\\')
		{
			out.push_back(c);
			continue;
		}

		if (!ReadEscape(out))
			return false;
	}

	return false;
}

bool Reader::ReadNumber(Value& out)
{
	char* end = nullptr;
	const double value = strtod(m_text.c_str() + m_at, &end);

	if (end == m_text.c_str() + m_at)
		return false;

	m_at = static_cast<size_t>(end - m_text.c_str());

	out.m_type = Value::Type_Number;
	out.m_number = value;
	return true;
}

bool Reader::ReadLiteral(const char* word, Value& out)
{
	const size_t length = strlen(word);

	if (m_text.compare(m_at, length, word) != 0)
		return false;

	m_at += length;

	if (strcmp(word, "null") == 0)
	{
		out.m_type = Value::Type_Null;
		return true;
	}

	out.m_type = Value::Type_Bool;
	out.m_bool = strcmp(word, "true") == 0;
	return true;
}

bool Reader::ReadArray(Value& out, size_t depth)
{
	out.m_type = Value::Type_Array;
	++m_at;

	SkipSpace();

	if (!AtEnd() && Peek() == ']')
	{
		++m_at;
		return true;
	}

	while (true)
	{
		Value item;

		if (!ReadValue(item, depth + 1))
			return false;

		out.m_items.push_back(std::move(item));

		SkipSpace();

		if (AtEnd())
			return false;

		const char c = m_text[m_at++];

		if (c == ']')
			return true;

		if (c != ',')
			return false;
	}
}

bool Reader::ReadObject(Value& out, size_t depth)
{
	out.m_type = Value::Type_Object;
	++m_at;

	SkipSpace();

	if (!AtEnd() && Peek() == '}')
	{
		++m_at;
		return true;
	}

	while (true)
	{
		SkipSpace();

		std::string key;

		if (!ReadString(key))
			return false;

		SkipSpace();

		if (AtEnd() || m_text[m_at++] != ':')
			return false;

		Value item;

		if (!ReadValue(item, depth + 1))
			return false;

		out.m_members.emplace_back(std::move(key), std::move(item));

		SkipSpace();

		if (AtEnd())
			return false;

		const char c = m_text[m_at++];

		if (c == '}')
			return true;

		if (c != ',')
			return false;
	}
}

bool Reader::ReadValue(Value& out, size_t depth)
{
	if (depth > m_depthLimit)
		return false;

	SkipSpace();

	if (AtEnd())
		return false;

	const char c = Peek();

	if (c == '{')
		return ReadObject(out, depth);

	if (c == '[')
		return ReadArray(out, depth);

	if (c == '"')
	{
		out.m_type = Value::Type_String;
		return ReadString(out.m_text);
	}

	if (c == 't')
		return ReadLiteral("true", out);

	if (c == 'f')
		return ReadLiteral("false", out);

	if (c == 'n')
		return ReadLiteral("null", out);

	return ReadNumber(out);
}

const Value* Value::Find(const char* key) const
{
	if (m_type != Type_Object || key == nullptr)
		return nullptr;

	for (const auto& member : m_members)
	{
		if (member.first == key)
			return &member.second;
	}

	return nullptr;
}

const Value* Value::At(size_t index) const
{
	if (m_type != Type_Array || index >= m_items.size())
		return nullptr;

	return &m_items[index];
}

std::string Value::AsString(const char* fallback) const
{
	return m_type == Type_String ? m_text : std::string(fallback);
}

double Value::AsNumber(double fallback) const
{
	return m_type == Type_Number ? m_number : fallback;
}

uint64_t Value::AsUnsigned(uint64_t fallback) const
{
	if (m_type != Type_Number || m_number < 0.0)
		return fallback;

	return static_cast<uint64_t>(m_number);
}

bool Value::AsBool(bool fallback) const
{
	return m_type == Type_Bool ? m_bool : fallback;
}

std::string Value::MemberString(const char* key, const char* fallback) const
{
	const Value* const found = Find(key);

	return found != nullptr ? found->AsString(fallback) : std::string(fallback);
}

uint64_t Value::MemberUnsigned(const char* key, uint64_t fallback) const
{
	const Value* const found = Find(key);

	return found != nullptr ? found->AsUnsigned(fallback) : fallback;
}

bool Value::MemberBool(const char* key, bool fallback) const
{
	const Value* const found = Find(key);

	return found != nullptr ? found->AsBool(fallback) : fallback;
}

bool Parse(const std::string& text, Value& out)
{
	out = Value();

	Reader reader(text, 64);

	if (!reader.ReadValue(out, 0))
		return false;

	reader.SkipSpace();
	return true;
}

}
