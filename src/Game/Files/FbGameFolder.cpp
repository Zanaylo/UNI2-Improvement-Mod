#include "Game/Files/FbGameFolder.h"

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

	if (Exists(root, "UNIEL.exe"))
		return Game_UNIEL;

	if (Exists(root, "Bgm\\bgm.txt") && (Exists(root, "RingGame.exe") || Exists(root, "eboot.bin")))
		return Game_DFCI;
	if (Exists(root, "BBTAG.exe"))
		return Game_BBTAG;

	if (Exists(root, "BBCF.exe"))
		return Game_BBCF;

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
	case Game_DFCI:
		return "DENGEKI BUNKO FIGHTING CLIMAX IGNITION";
	case Game_UNIEL:
		return "UNDER NIGHT IN-BIRTH Exe:Late";
	case Game_BBTAG:
		return "BLAZBLUE CROSS TAG BATTLE";
	case Game_BBCF:
		return "BLAZBLUE CENTRALFICTION";
	default:
		return "nothing the mod knows";
	}
}
