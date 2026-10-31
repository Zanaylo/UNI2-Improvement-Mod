#include "Core/Text/LabelText.h"

#include <cstdio>
#include <cstring>

namespace {

const char* const kIdMark = "##";
const char* const kOwnIdMark = "###";

}

std::string_view LabelText::Visible(const char* label)
{
	if (label == nullptr)
		return std::string_view();

	const char* const mark = strstr(label, kIdMark);

	return mark != nullptr ? std::string_view(label, static_cast<size_t>(mark - label)) : std::string_view(label);
}

bool LabelText::Compose(const char* shown, const char* label, char* out, size_t size)
{
	if (shown == nullptr || label == nullptr || out == nullptr || size == 0)
		return false;

	const char* const mark = strstr(label, kIdMark);
	const bool keepsOwnId = mark != nullptr && strncmp(mark, kOwnIdMark, strlen(kOwnIdMark)) == 0;
	const int written = keepsOwnId ? snprintf(out, size, "%s%s", shown, mark)
		: snprintf(out, size, "%s%s%s", shown, kOwnIdMark, label);

	return written > 0 && static_cast<size_t>(written) < size;
}
