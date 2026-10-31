#pragma once

namespace MainTabs
{
	enum Tab
	{
		Tab_Training,
		Tab_Visuals,
		Tab_Audio,
		Tab_Online,
		Tab_Mod,
		Tab_Debug,
		Tab_Config,
		Tab_COUNT
	};

	struct Shown
	{
		bool training;
		bool music;
		bool voices;
		bool subtitles;
		bool netplay;
		bool patches;
		bool replays;
		bool modPacks;
	};

	bool Visible(Tab tab, const Shown& shown);
}
