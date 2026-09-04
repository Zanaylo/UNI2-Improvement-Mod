#pragma once

#include "Core/AsyncFileDialog.h"

class StagesPanel
{
public:
	void Draw();

private:
	void DrawHidden();
	void DrawSource();
	void DrawOffers();
	void DrawOfferRow(int index);
	void DrawPorted();
	void DrawRestart();

	AsyncFileDialog m_sourceDialog;
	char m_name[96] = {};
	int m_chosen = -1;
};
