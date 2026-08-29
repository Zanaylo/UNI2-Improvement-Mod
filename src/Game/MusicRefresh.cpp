#include "Game/MusicRefresh.h"

#include "Game/BgmLibrary.h"
#include "Game/BgmThemes.h"
#include "Game/ModFiles.h"
#include "Game/UserMusic.h"

void MusicRefresh::Reindex()
{
	ModFiles::Rescan();
	BgmLibrary::Load();
}

void MusicRefresh::Rescan()
{
	UserMusic::Scan();
	Reindex();
	BgmThemes::Reload();
}
