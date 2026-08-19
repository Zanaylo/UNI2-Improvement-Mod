#include "Core/keycodes.h"

#include <Windows.h>

namespace {

struct KeyName
{
	const char* name;
	int virtualKey;
};

constexpr KeyName kKeys[] = {
	{ "F1", VK_F1 }, { "F2", VK_F2 }, { "F3", VK_F3 }, { "F4", VK_F4 },
	{ "F5", VK_F5 }, { "F6", VK_F6 }, { "F7", VK_F7 }, { "F8", VK_F8 },
	{ "F9", VK_F9 }, { "F10", VK_F10 }, { "F11", VK_F11 }, { "F12", VK_F12 },
	{ "A", 'A' }, { "B", 'B' }, { "C", 'C' }, { "D", 'D' }, { "E", 'E' },
	{ "F", 'F' }, { "G", 'G' }, { "H", 'H' }, { "I", 'I' }, { "J", 'J' },
	{ "K", 'K' }, { "L", 'L' }, { "M", 'M' }, { "N", 'N' }, { "O", 'O' },
	{ "P", 'P' }, { "Q", 'Q' }, { "R", 'R' }, { "S", 'S' }, { "T", 'T' },
	{ "U", 'U' }, { "V", 'V' }, { "W", 'W' }, { "X", 'X' }, { "Y", 'Y' },
	{ "Z", 'Z' },
	{ "0", '0' }, { "1", '1' }, { "2", '2' }, { "3", '3' }, { "4", '4' },
	{ "5", '5' }, { "6", '6' }, { "7", '7' }, { "8", '8' }, { "9", '9' },
	{ "NUM0", VK_NUMPAD0 }, { "NUM1", VK_NUMPAD1 }, { "NUM2", VK_NUMPAD2 },
	{ "NUM3", VK_NUMPAD3 }, { "NUM4", VK_NUMPAD4 }, { "NUM5", VK_NUMPAD5 },
	{ "NUM6", VK_NUMPAD6 }, { "NUM7", VK_NUMPAD7 }, { "NUM8", VK_NUMPAD8 },
	{ "NUM9", VK_NUMPAD9 },
	{ "INSERT", VK_INSERT }, { "DELETE", VK_DELETE }, { "HOME", VK_HOME },
	{ "END", VK_END }, { "PAGEUP", VK_PRIOR }, { "PAGEDOWN", VK_NEXT },
	{ "LEFT", VK_LEFT }, { "RIGHT", VK_RIGHT }, { "UP", VK_UP }, { "DOWN", VK_DOWN },
	{ "SPACE", VK_SPACE }, { "TAB", VK_TAB }, { "ENTER", VK_RETURN },
	{ "BACKSPACE", VK_BACK }, { "ESCAPE", VK_ESCAPE },
	{ "SHIFT", VK_SHIFT }, { "CTRL", VK_CONTROL }, { "ALT", VK_MENU },
	{ "NONE", 0 },
};

}

int GetVirtualKeyFromName(const std::string& name)
{
	const size_t first = name.find_first_not_of(" \t");
	if (first == std::string::npos)
		return 0;

	const size_t last = name.find_last_not_of(" \t");
	std::string trimmed = name.substr(first, last - first + 1);

	if (_strnicmp(trimmed.c_str(), "Fn+", 3) == 0)
		trimmed.erase(0, 3);

	for (const KeyName& key : kKeys)
	{
		if (_stricmp(key.name, trimmed.c_str()) == 0)
			return key.virtualKey;
	}

	return 0;
}

const char* GetNameFromVirtualKey(int virtualKey)
{
	for (const KeyName& key : kKeys)
	{
		if (key.virtualKey == virtualKey)
			return key.name;
	}

	return "UNKNOWN";
}

int PollPressedKey()
{
	for (const KeyName& key : kKeys)
	{
		if (key.virtualKey != 0 && (GetAsyncKeyState(key.virtualKey) & 0x8000) != 0)
			return key.virtualKey;
	}

	return 0;
}
