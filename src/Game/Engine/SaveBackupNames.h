#pragma once

#include <string>
#include <vector>

namespace SaveBackupNames
{
	std::string FileName(const std::string& day);
	std::string AccountOf(const std::string& savePath);
	bool IsBackup(const std::string& name);
	bool HasDay(const std::vector<std::string>& names, const std::string& day);
	std::vector<std::string> Expired(const std::vector<std::string>& names, size_t keep);
}
