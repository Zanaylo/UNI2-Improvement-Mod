#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/Stages/StageImport.h"
#include "Overlay/Panels/StageFieldsWindow.h"

#include <string>
#include <vector>

class StagesPanel
{
public:
	void Draw();

private:
	struct Row
	{
		char name[StageImport::kNameInput];
	};

	void DrawHidden();
	void DrawSource();
	void DrawArcsys();
	void DrawCustom();
	void DrawReplace();
	void DrawOffers();
	void DrawOfferRow(int index);
	void DrawBackdropToggle();
	bool Offered(int index) const;
	void DrawRoom();
	void DrawPlacement();
	void DrawPorted();
	void DrawRename(int id);
	void BeginRename(int id, const std::string& name);
	void DrawTuning(int key, int slot);
	void DrawHelp();
	void DrawRestart();

	void SyncRows();
	void PumpQueue();
	void Queue(int index);

	AsyncFileDialog m_sourceDialog;
	AsyncFileDialog m_customDialog;
	AsyncFileDialog m_replaceDialog;
	StageFieldsWindow m_fields;
	int m_replaceNumber = 0;
	std::vector<Row> m_rows;
	std::vector<int> m_queue;
	std::string m_rowsFor;
	int m_rowCount = 0;
	int m_renaming = -1;
	bool m_focusName = false;
	char m_newName[StageImport::kNameInput] = {};
};
