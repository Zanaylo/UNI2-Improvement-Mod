#pragma once

#include <string>
#include <vector>

namespace ConfigFiles
{
	enum class Outcome
	{
		Ready,
		Migrated,
		Merged,
		Fresh,
		Failed,
	};

	struct Result
	{
		Outcome kind;
		int files;
		int fromRevision;
		std::string backup;
		std::vector<std::string> notes;
	};

	Result PrepareIn(const std::string& root, const std::string& legacyPath, const std::string& defaults);
}
