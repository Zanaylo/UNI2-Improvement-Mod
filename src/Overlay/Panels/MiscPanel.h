#pragma once

#include "Core/AsyncFileDialog.h"

class MiscPanel
{
public:
	void Draw();

private:
	void DrawPortraits();
	void DrawPortraitDownload();
	void DrawPortraitStatus();
	void DrawPortraitPicks();
	void DrawPortraitStyle();
	void DrawPortraitPick(int chara);
	void WearStyle(int option);
	void DrawAnnouncers();
	void DrawAnnouncerPicks();

	AsyncFileDialog m_announcers;
	bool m_portraitsSeen = false;
	int m_portraitStyle = -1;
};
