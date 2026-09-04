#include "Game/FbGameFolder.h"

#include <string>

#include <Windows.h>

namespace {

bool Exists(const std::string& folder, const char* name)
{
	std::string path = folder;

	if (!path.empty() && path.back() != '\\' && path.back() != '/')
		path.push_back('\\');

	path += name;

	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

}

FbGameFolder::Game FbGameFolder::Detect(const char* folder)
{
	if (folder == nullptr || folder[0] == 0)
		return Game_None;

	const std::string root = folder;

	if (Exists(root, "MBAA.exe") && Exists(root, "0001.p"))
		return Game_MBAA;

	if (Exists(root, "MBTL.exe"))
		return Game_MBTL;

	if (Exists(root, "UNIst.exe") || Exists(root, "UNIclr.exe"))
		return Game_UNI;

	return Game_None;
}

const char* FbGameFolder::Name(Game game)
{
	switch (game)
	{
	case Game_UNI:
		return "UNDER NIGHT IN-BIRTH";
	case Game_MBTL:
		return "MELTY BLOOD: TYPE LUMINA";
	case Game_MBAA:
		return "MELTY BLOOD Actress Again Current Code";
	default:
		return "nothing the mod knows";
	}
}
