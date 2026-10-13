#include "Core/Harness/GameDebugCommands.h"

#include "Game/Debug/GameDebug.h"

#include <cstdlib>
#include <cstring>

namespace {

int FindOption(const std::string& key)
{
	for (int i = 0; i < GameDebug::OptionCount(); ++i)
	{
		if (_stricmp(GameDebug::OptionAt(i).key, key.c_str()) == 0)
			return i;
	}

	return -1;
}

std::string Set(const std::vector<std::string>& words)
{
	const int index = FindOption(words[1]);

	if (index < 0)
		return "error no game debug option " + words[1];

	*GameDebug::OptionAt(index).value = atoi(words[2].c_str()) != 0;
	GameDebug::Save(index);

	return std::string("ok ") + GameDebug::StatusText();
}

}

bool GameDebugCommands::Execute(const std::vector<std::string>& words, std::string& reply)
{
	if (words[0] != "gamedebug")
		return false;

	if (!GameDebug::IsEnabled())
	{
		reply = "error the game debug options are off, set [Debug] GameDebug = 1";
		return true;
	}

	reply = words.size() >= 3 ? Set(words) : std::string("ok ") + GameDebug::StatusText();
	return true;
}
