#pragma once

#include <cstddef>
#include <string_view>

namespace LabelText
{
	std::string_view Visible(const char* label);

	bool Compose(const char* shown, const char* label, char* out, size_t size);
}
