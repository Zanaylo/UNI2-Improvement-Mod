#pragma once

#include "Core/AsyncFileDialog.h"

#include <string>
#include <vector>

class StagesPanel
{
public:
	void Draw();

private:
	struct Row
	{
		char name[96];
	};

	void DrawHidden();
	void DrawSource();
	void DrawCustom();
	void DrawOffers();
	void DrawOfferRow(int index);
	void DrawRoom();
	void DrawPorted();
	void DrawHelp();
	void DrawRestart();

	void SyncRows();
	void PumpQueue();
	void Queue(int index);

	AsyncFileDialog m_sourceDialog;
	AsyncFileDialog m_customDialog;
	std::vector<Row> m_rows;
	std::vector<int> m_queue;
	std::string m_rowsFor;
	int m_rowCount = 0;
};
