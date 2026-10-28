#pragma once

#include <cstdint>
#include <string>

namespace SquirrelText
{
	bool LeadByte(uint8_t byte);
	size_t Skip(const std::string& text, size_t at, const char* of);
	size_t MatchPair(const std::string& text, size_t open);
	size_t ValueEnd(const std::string& block, size_t value);
	bool FieldSpan(const std::string& block, const char* key, size_t& valueAt, size_t& valueEnd);
	bool Field(const std::string& block, const char* key, std::string& out);
	std::string Unquoted(const std::string& value);
}
