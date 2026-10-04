#pragma once

#include "Game/Audio/OstPac.h"

#include <cstdint>
#include <string>
#include <vector>

class MbaaArchive
{
public:
	bool Open(const std::string& folder);

	std::vector<OstPac::Entry> List(const char* folder) const;
	bool Read(const char* folder, const char* name, std::vector<uint8_t>& out) const;

private:
	std::vector<OstPac::Archive> m_archives;
};
