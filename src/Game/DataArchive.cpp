#include "Game/DataArchive.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameArchive.h"

#include <Windows.h>

#include <mutex>

namespace {

std::mutex g_lock;
GameArchive g_archive;
bool g_built = false;

std::string ArchiveDirectory()
{
	return GetModDirectory() + "d\\";
}

GameArchive& Opened()
{
	if (g_built)
		return g_archive;

	g_built = true;
	g_archive.Open(ArchiveDirectory());

	return g_archive;
}

}

bool DataArchive::IsAvailable()
{
	const std::string directory = ArchiveDirectory();
	const DWORD attributes = GetFileAttributesA(directory.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool DataArchive::Read(const char* folder, const char* file, std::vector<uint8_t>& out)
{
	out.clear();

	if (folder == nullptr || file == nullptr || !IsAvailable())
		return false;

	std::lock_guard<std::mutex> guard(g_lock);

	if (!Opened().Read(folder, file, out))
		return false;

	LOG("archive: %s\\%s is %d bytes", folder, file, static_cast<int>(out.size()));
	return true;
}

bool DataArchive::List(const char* folder, std::vector<std::string>& out)
{
	out.clear();

	if (folder == nullptr || !IsAvailable())
		return false;

	std::lock_guard<std::mutex> guard(g_lock);
	return Opened().List(folder, out);
}

void DataArchive::Folders(std::vector<std::string>& out)
{
	out.clear();

	if (!IsAvailable())
		return;

	std::lock_guard<std::mutex> guard(g_lock);
	Opened().Folders(out);
}
