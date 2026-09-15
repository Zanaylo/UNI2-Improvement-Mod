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
	void DrawArcsys();
	void DrawCustom();
	void DrawReplace();
	void DrawOffers();
	void DrawOfferRow(int index);
	void DrawRoom();
	void DrawPlacement();
	void DrawPorted();
	void DrawHelp();
	void DrawRestart();

	bool Number(const char* id, float* value, float low, float high, const char* format);

	void SyncRows();
	void PumpQueue();
	void Queue(int index);

	AsyncFileDialog m_sourceDialog;
	AsyncFileDialog m_customDialog;
	AsyncFileDialog m_replaceDialog;
	int m_replaceNumber = 0;
	std::vector<Row> m_rows;
	std::vector<int> m_queue;
	std::string m_rowsFor;
	int m_rowCount = 0;
	unsigned int m_pressed = 0;
	float m_pressedValue = 0.0f;
};
