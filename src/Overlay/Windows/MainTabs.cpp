#include "Overlay/Windows/MainTabs.h"

bool MainTabs::Visible(Tab tab, const Shown& shown)
{
	switch (tab)
	{
	case Tab_Training:
		return shown.training;

	case Tab_Audio:
		return shown.music || shown.voices || shown.subtitles;

	case Tab_Online:
		return shown.netplay || shown.patches || shown.replays;

	case Tab_Mod:
		return shown.modPacks || shown.gameDebug;

	default:
		return true;
	}
}
