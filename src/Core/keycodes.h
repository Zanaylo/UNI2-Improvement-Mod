#pragma once

#include <string>

int GetVirtualKeyFromName(const std::string& name);
const char* GetNameFromVirtualKey(int virtualKey);

int PollPressedKey();
