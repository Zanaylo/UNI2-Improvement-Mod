#include "Game/Stages/SquirrelText.h"

#include <cctype>
#include <cstring>

namespace {

size_t ValueStart(const std::string& block, size_t after)
{
	const size_t sameLine = SquirrelText::Skip(block, after, " \t");
	const size_t anyLine = SquirrelText::Skip(block, after, " \t\r\n");

	return anyLine < block.size() && block[anyLine] == '[' ? anyLine : sameLine;
}

bool KeyAt(const std::string& text, size_t at, const char* key, size_t length)
{
	if (at + length > text.size() || text.compare(at, length, key) != 0)
		return false;

	const char before = at == 0 ? ' ' : text[at - 1];
	const char after = at + length >= text.size() ? ' ' : text[at + length];

	return isalnum(static_cast<unsigned char>(before)) == 0 && before != '_' &&
		isalnum(static_cast<unsigned char>(after)) == 0 && after != '_';
}

}

bool SquirrelText::LeadByte(uint8_t byte)
{
	return (byte >= 0x81 && byte <= 0x9f) || (byte >= 0xe0 && byte <= 0xfc);
}

size_t SquirrelText::Skip(const std::string& text, size_t at, const char* of)
{
	while (at < text.size() && strchr(of, text[at]) != nullptr && text[at] != 0)
		++at;

	return at;
}

size_t SquirrelText::MatchPair(const std::string& text, size_t open)
{
	const char opener = open < text.size() ? text[open] : 0;
	const char closer = opener == '{' ? '}' : (opener == '[' ? ']' : 0);

	if (closer == 0)
		return std::string::npos;

	int depth = 0;

	for (size_t at = open; at < text.size(); ++at)
	{
		const uint8_t byte = static_cast<uint8_t>(text[at]);

		if (LeadByte(byte) && at + 1 < text.size())
		{
			++at;
			continue;
		}

		depth += byte == opener ? 1 : (byte == closer ? -1 : 0);

		if (depth == 0)
			return at + 1;
	}

	return std::string::npos;
}

size_t SquirrelText::ValueEnd(const std::string& block, size_t value)
{
	if (value < block.size() && block[value] == '[')
		return MatchPair(block, value);

	size_t end = value;

	while (end < block.size() && block[end] != ',' && block[end] != '\n' && block[end] != '\r' &&
		!(block[end] == '/' && end + 1 < block.size() && block[end + 1] == '/'))
	{
		++end;
	}

	return end;
}

bool SquirrelText::FieldSpan(const std::string& block, const char* key, size_t& valueAt, size_t& valueEnd)
{
	const size_t length = strlen(key);

	for (size_t at = block.find(key); at != std::string::npos; at = block.find(key, at + 1))
	{
		if (!KeyAt(block, at, key, length))
			continue;

		const size_t equals = Skip(block, at + length, " \t");

		if (equals >= block.size() || block[equals] != '=')
			continue;

		valueAt = ValueStart(block, equals + 1);
		valueEnd = ValueEnd(block, valueAt);

		if (valueEnd == std::string::npos)
			return false;

		while (valueEnd > valueAt && (block[valueEnd - 1] == ' ' || block[valueEnd - 1] == '\t'))
			--valueEnd;

		return valueEnd > valueAt;
	}

	return false;
}

bool SquirrelText::Field(const std::string& block, const char* key, std::string& out)
{
	size_t valueAt = 0;
	size_t valueEnd = 0;

	if (!FieldSpan(block, key, valueAt, valueEnd))
		return false;

	out = block.substr(valueAt, valueEnd - valueAt);
	return true;
}

std::string SquirrelText::Unquoted(const std::string& value)
{
	if (value.empty() || value.front() != '"')
		return value;

	return value.substr(1, value.size() - (value.size() > 1 && value.back() == '"' ? 2 : 1));
}
