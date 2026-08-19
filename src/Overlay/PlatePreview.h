#pragma once

#include "Game/PlayerCard.h"

#include <d3d9.h>

class PlatePreview
{
public:
	PlatePreview() = default;
	~PlatePreview();

	PlatePreview(const PlatePreview&) = delete;
	PlatePreview& operator=(const PlatePreview&) = delete;

	void Draw(float width);
	void Release();

private:
	struct Layer
	{
		IDirect3DTexture9* texture = nullptr;
		int id = -1;
		bool resolved = false;
	};

	bool Ensure(PlayerCard::PlateLayer layer, int id);
	void Load(Layer& slot, PlayerCard::PlateLayer layer, int id);

	Layer m_layers[PlayerCard::kLayerCount];
};
