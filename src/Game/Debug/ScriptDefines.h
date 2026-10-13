#pragma once

#include <string>

namespace ScriptDefines
{
	struct Value
	{
		const char* name;
		int value;
	};

	bool Rewrite(const std::string& source, const Value* values, int count, std::string& out, std::string& missing);
}
