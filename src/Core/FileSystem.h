#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out, size_t minimumSize = 0);

bool WriteWholeFile(const std::string& path, const std::vector<uint8_t>& data);

bool SameFolder(const std::string& left, const std::string& right);

std::vector<std::string> ListFileNames(const std::string& folder);

bool CopyDirectoryTree(const std::string& from, const std::string& to, int& outFiles);
bool MoveToRecycleBin(const std::string& path);
