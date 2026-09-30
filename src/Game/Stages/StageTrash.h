#pragma once

#include <string>

namespace StageTrash
{
	void Discard(const std::string& folder);
	void DeleteNow(const std::string& folder);
	bool Pending(const std::string& folder);
	void Sweep();
}
