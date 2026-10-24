#pragma once

#include "Core/Config/IniDocument.h"

#include <map>
#include <string>

namespace ConfigSplit
{
	std::map<std::string, std::string> Split(const IniDocument& whole);
}
