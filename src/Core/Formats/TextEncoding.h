#pragma once

#include <cstddef>
#include <string>

namespace TextEncoding
{
	bool Utf8ToCodePage(const std::string& utf8, unsigned int codePage, std::string& out,
		bool* outLossy = nullptr);

	bool Utf8ToShiftJis(const std::string& utf8, std::string& out, bool* outLossy = nullptr);

	bool ShiftJisToUtf8(const char* text, size_t maxBytes, std::string& out);

	size_t ShiftJisBoundary(const std::string& shiftJis, size_t maxBytes);
}
