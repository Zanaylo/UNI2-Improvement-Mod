#pragma once

#include <string>

namespace LegacyFolder
{
	enum class Outcome
	{
		NothingToMove,
		Moved,
		BothKept,
		Failed
	};

	Outcome Adopt(const std::string& legacy, const std::string& current);
}
