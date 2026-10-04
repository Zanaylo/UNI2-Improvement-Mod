#include "Game/Files/MbaaArchive.h"

#include <Windows.h>

namespace {

constexpr const char* kArchives = "*.p";

std::string Combine(const std::string& folder, const char* name)
{
	return folder.empty() || folder.back() == '\\' ? folder + name : folder + "\\" + name;
}

bool SameFolder(const std::string& folder, const char* wanted)
{
	return wanted == nullptr || _stricmp(folder.c_str(), wanted) == 0;
}

}

bool MbaaArchive::Open(const std::string& folder)
{
	m_archives.clear();

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(folder, kArchives).c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		OstPac::Archive archive;

		if (archive.Open(Combine(folder, found.cFileName)))
			m_archives.push_back(archive);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return !m_archives.empty();
}

std::vector<OstPac::Entry> MbaaArchive::List(const char* folder) const
{
	std::vector<OstPac::Entry> out;

	for (const OstPac::Archive& archive : m_archives)
	{
		for (int i = 0; i < archive.Count(); ++i)
		{
			const OstPac::Entry& entry = archive.At(i);

			if (SameFolder(entry.folder, folder))
				out.push_back(entry);
		}
	}

	return out;
}

bool MbaaArchive::Read(const char* folder, const char* name, std::vector<uint8_t>& out) const
{
	out.clear();

	for (const OstPac::Archive& archive : m_archives)
	{
		for (int i = 0; i < archive.Count(); ++i)
		{
			const OstPac::Entry& entry = archive.At(i);

			if (_stricmp(entry.name.c_str(), name) == 0 && SameFolder(entry.folder, folder))
				return archive.Read(entry, out);
		}
	}

	return false;
}
