#include "Core/LegacyFolder.h"

#include <Windows.h>

namespace {

bool IsFolder(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

}

LegacyFolder::Outcome LegacyFolder::Adopt(const std::string& legacy, const std::string& current)
{
	if (!IsFolder(legacy))
		return Outcome::NothingToMove;

	if (Exists(current))
		return Outcome::BothKept;

	return MoveFileA(legacy.c_str(), current.c_str()) != 0 ? Outcome::Moved : Outcome::Failed;
}
