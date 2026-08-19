#include "Core/TextEncoding.h"

#include <Windows.h>

#include <vector>

namespace {

constexpr UINT kShiftJis = 932;

bool IsLeadByte(unsigned char value)
{
	return (value >= 0x81 && value <= 0x9F) || (value >= 0xE0 && value <= 0xFC);
}

bool ToWide(UINT codePage, const char* text, int length, std::wstring& out)
{
	if (text == nullptr || length <= 0)
	{
		out.clear();
		return true;
	}

	const int needed = MultiByteToWideChar(codePage, 0, text, length, nullptr, 0);
	if (needed <= 0)
		return false;

	out.resize(static_cast<size_t>(needed));

	return MultiByteToWideChar(codePage, 0, text, length, &out[0], needed) == needed;
}

}

bool TextEncoding::Utf8ToShiftJis(const std::string& utf8, std::string& out, bool* outLossy)
{
	if (outLossy != nullptr)
		*outLossy = false;

	out.clear();

	if (utf8.empty())
		return true;

	std::wstring wide;
	if (!ToWide(CP_UTF8, utf8.c_str(), static_cast<int>(utf8.size()), wide))
		return false;

	const int needed = WideCharToMultiByte(kShiftJis, 0, wide.c_str(), static_cast<int>(wide.size()),
		nullptr, 0, nullptr, nullptr);
	if (needed <= 0)
		return false;

	out.resize(static_cast<size_t>(needed));

	BOOL usedDefault = FALSE;
	const char fallback = '?';

	const int written = WideCharToMultiByte(kShiftJis, 0, wide.c_str(), static_cast<int>(wide.size()),
		&out[0], needed, &fallback, &usedDefault);
	if (written != needed)
	{
		out.clear();
		return false;
	}

	if (outLossy != nullptr)
		*outLossy = usedDefault != FALSE;

	return true;
}

bool TextEncoding::ShiftJisToUtf8(const char* text, size_t maxBytes, std::string& out)
{
	out.clear();

	if (text == nullptr || maxBytes == 0)
		return true;

	size_t length = 0;
	while (length < maxBytes && text[length] != '\0')
		++length;

	if (length == 0)
		return true;

	std::wstring wide;
	if (!ToWide(kShiftJis, text, static_cast<int>(length), wide))
		return false;

	const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
		nullptr, 0, nullptr, nullptr);
	if (needed <= 0)
		return false;

	out.resize(static_cast<size_t>(needed));

	return WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), &out[0],
		needed, nullptr, nullptr) == needed;
}

size_t TextEncoding::ShiftJisBoundary(const std::string& shiftJis, size_t maxBytes)
{
	size_t offset = 0;

	while (offset < shiftJis.size())
	{
		const unsigned char lead = static_cast<unsigned char>(shiftJis[offset]);
		const size_t width = IsLeadByte(lead) && offset + 1 < shiftJis.size() ? 2 : 1;

		if (offset + width > maxBytes)
			break;

		offset += width;
	}

	return offset;
}
