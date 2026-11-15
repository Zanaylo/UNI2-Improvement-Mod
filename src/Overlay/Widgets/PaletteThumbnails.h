#pragma once

#include "Core/BackgroundJob.h"
#include "Game/Customize/SheetSamples.h"

#include <imgui.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

struct IDirect3DTexture9;

class PaletteThumbnails
{
public:
	PaletteThumbnails();

	void BeginFrame();
	void Release();
	void ForgetPalettes();

	bool DrawCard(ImDrawList* draw, int chara, const uint8_t* rgba, const ImVec2& corner, const ImVec2& size);
	bool DrawSheet(int chara, const uint8_t* rgba, float maxWidth);

private:
	enum class SheetState
	{
		Unknown,
		Decoding,
		Ready,
		Missing,
	};

	struct Sheet
	{
		SheetState state = SheetState::Unknown;
		SheetSamples::Samples card;
		SheetSamples::Samples large;
	};

	struct Decoded
	{
		int chara = -1;
		SheetSamples::Samples card;
		SheetSamples::Samples large;
	};

	struct Thumbnail
	{
		IDirect3DTexture9* texture = nullptr;
		int lastFrame = 0;
	};

	Sheet* SheetFor(int chara);
	void TakeDecoded();
	void StartDecoding(int chara);

	IDirect3DTexture9* TextureFor(const uint8_t* rgba, const SheetSamples::Samples& samples);
	void MakeRoom();

	IDirect3DTexture9* LargeFor(int chara, const uint8_t* rgba, const SheetSamples::Samples& samples);

	std::vector<Sheet> m_sheets;
	std::unordered_map<const uint8_t*, Thumbnail> m_cards;

	IDirect3DTexture9* m_large = nullptr;
	const uint8_t* m_largeColours = nullptr;
	int m_largeChara = -1;

	std::shared_ptr<Decoded> m_decoded;
	BackgroundJob m_decodeJob;

	int m_frame = 0;
	int m_uploads = 0;
	bool m_decoding = false;
};
