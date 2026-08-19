#pragma once

#include "Game/PlayerCard.h"
#include "Overlay/PlatePreview.h"

class PlayerCardPanel
{
public:
	void Draw();

private:
	void DrawPreview();
	void DrawTitle();
	void DrawPlate();
	void DrawLayer(PlayerCard::PlateLayer layer);
	void StepLayer(PlayerCard::PlateLayer layer, int current, int steps);

	void PullTitle();
	void PushTitle();

	void SetStatus(const char* text);

	bool m_titlePulled = false;
	int m_activeLayer = -1;
	int m_pendingId[PlayerCard::kLayerCount] = {};
	char m_title[192] = {};
	char m_status[192] = {};

	PlatePreview m_preview;
};
