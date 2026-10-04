#pragma once

#include "Core/AsyncFileDialog.h"

class MiscPanel
{
public:
	void Draw();

private:
	void DrawPortraits();
	void DrawPortraitPicks();
	void DrawAnnouncers();
	void DrawAnnouncerPicks();

	AsyncFileDialog m_portraits;
	AsyncFileDialog m_announcers;
};
