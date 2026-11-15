#include "Overlay/Widgets/PaletteThumbnails.h"

#include "Core/Formats/PngImage.h"
#include "D3D9/Device/DeviceHooks.h"
#include "D3D9/Draw/TextureGraveyard.h"
#include "Game/Customize/BasePals.h"
#include "Game/Tables/CharaTables.h"

#include <d3d9.h>

namespace {

constexpr SheetSamples::Rect kCardCrop = { 150, 500, 800, 800 };
constexpr int kCardSide = 128;
constexpr int kLargeWidth = 400;
constexpr int kLargeHeight = 347;
constexpr int kUploadsPerFrame = 2;
constexpr size_t kMostCards = 96;

ImTextureID Handle(IDirect3DTexture9* texture)
{
	return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture));
}

IDirect3DTexture9* Upload(const SheetSamples::Samples& samples, const uint8_t* rgba)
{
	IDirect3DDevice9* const device = DeviceHooks::GetDevice();

	if (device == nullptr)
		return nullptr;

	IDirect3DTexture9* texture = nullptr;

	if (FAILED(device->CreateTexture(samples.width, samples.height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
		return nullptr;

	D3DLOCKED_RECT locked = {};

	if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
	{
		texture->Release();
		return nullptr;
	}

	SheetSamples::Paint(samples, rgba, static_cast<uint8_t*>(locked.pBits), locked.Pitch);
	texture->UnlockRect(0);

	return texture;
}

}

PaletteThumbnails::PaletteThumbnails()
	: m_decodeJob("palette thumbnails")
{
}

void PaletteThumbnails::BeginFrame()
{
	++m_frame;
	m_uploads = 0;

	TakeDecoded();
}

void PaletteThumbnails::Release()
{
	ForgetPalettes();

	m_decoded.reset();

	m_sheets.clear();
	m_sheets.shrink_to_fit();
}

void PaletteThumbnails::ForgetPalettes()
{
	for (const auto& card : m_cards)
		TextureGraveyard::Bury(card.second.texture);

	m_cards.clear();

	TextureGraveyard::Bury(m_large);

	m_large = nullptr;
	m_largeColours = nullptr;
	m_largeChara = -1;
}

PaletteThumbnails::Sheet* PaletteThumbnails::SheetFor(int chara)
{
	if (chara < 0 || chara >= CharaTables::GetCharaCount())
		return nullptr;

	if (m_sheets.empty())
		m_sheets.resize(static_cast<size_t>(CharaTables::GetCharaCount()));

	Sheet& sheet = m_sheets[static_cast<size_t>(chara)];

	if (sheet.state == SheetState::Unknown)
		StartDecoding(chara);

	return &sheet;
}

void PaletteThumbnails::TakeDecoded()
{
	if (!m_decoding || m_decodeJob.IsBusy())
		return;

	m_decoding = false;

	const std::shared_ptr<Decoded> decoded = std::move(m_decoded);
	const bool finished = m_decodeJob.ConsumeFinished();

	if (decoded == nullptr || decoded->chara < 0 || static_cast<size_t>(decoded->chara) >= m_sheets.size())
		return;

	Sheet& sheet = m_sheets[static_cast<size_t>(decoded->chara)];

	if (!finished)
	{
		sheet.state = SheetState::Missing;
		return;
	}

	sheet.card = std::move(decoded->card);
	sheet.large = std::move(decoded->large);
	sheet.state = SheetState::Ready;
}

void PaletteThumbnails::StartDecoding(int chara)
{
	if (m_decoding)
		return;

	const uint8_t* data = nullptr;
	size_t size = 0;

	if (!BasePals::Get(chara, BasePals::kCharacterSet, data, size))
	{
		m_sheets[static_cast<size_t>(chara)].state = SheetState::Missing;
		return;
	}

	std::shared_ptr<Decoded> decoded = std::make_shared<Decoded>();

	decoded->chara = chara;
	m_decoded = decoded;

	const bool started = m_decodeJob.Start([decoded, data, size]()
	{
		const std::vector<uint8_t> blob(data, data + size);
		std::vector<uint8_t> indices;
		int width = 0;
		int height = 0;

		if (!PngImage::DecodeIndices(blob, width, height, indices))
			return false;

		decoded->card = SheetSamples::Build(indices.data(), width, height, kCardCrop, kCardSide, kCardSide);
		decoded->large = SheetSamples::Build(indices.data(), width, height, SheetSamples::Rect{ 0, 0, width, height },
			kLargeWidth, kLargeHeight);

		return true;
	});

	if (!started)
	{
		m_decoded.reset();
		return;
	}

	m_decoding = true;
	m_sheets[static_cast<size_t>(chara)].state = SheetState::Decoding;
}

void PaletteThumbnails::MakeRoom()
{
	if (m_cards.size() < kMostCards)
		return;

	auto oldest = m_cards.end();

	for (auto card = m_cards.begin(); card != m_cards.end(); ++card)
	{
		if (card->second.lastFrame != m_frame && (oldest == m_cards.end() || card->second.lastFrame < oldest->second.lastFrame))
			oldest = card;
	}

	if (oldest == m_cards.end())
		return;

	TextureGraveyard::Bury(oldest->second.texture);
	m_cards.erase(oldest);
}

IDirect3DTexture9* PaletteThumbnails::TextureFor(const uint8_t* rgba, const SheetSamples::Samples& samples)
{
	const auto found = m_cards.find(rgba);

	if (found != m_cards.end())
	{
		found->second.lastFrame = m_frame;
		return found->second.texture;
	}

	if (m_uploads >= kUploadsPerFrame)
		return nullptr;

	++m_uploads;

	IDirect3DTexture9* const texture = Upload(samples, rgba);

	if (texture == nullptr)
		return nullptr;

	MakeRoom();

	m_cards[rgba] = Thumbnail{ texture, m_frame };

	return texture;
}

bool PaletteThumbnails::DrawCard(ImDrawList* draw, int chara, const uint8_t* rgba, const ImVec2& corner, const ImVec2& size)
{
	const Sheet* const sheet = SheetFor(chara);

	if (sheet == nullptr || sheet->state != SheetState::Ready)
		return false;

	IDirect3DTexture9* const texture = TextureFor(rgba, sheet->card);

	if (texture == nullptr)
		return false;

	draw->AddImage(Handle(texture), corner, ImVec2(corner.x + size.x, corner.y + size.y));

	return true;
}

IDirect3DTexture9* PaletteThumbnails::LargeFor(int chara, const uint8_t* rgba, const SheetSamples::Samples& samples)
{
	if (m_large != nullptr && m_largeColours == rgba && m_largeChara == chara)
		return m_large;

	IDirect3DTexture9* const texture = Upload(samples, rgba);

	if (texture == nullptr)
		return m_large;

	TextureGraveyard::Bury(m_large);

	m_large = texture;
	m_largeColours = rgba;
	m_largeChara = chara;

	return m_large;
}

bool PaletteThumbnails::DrawSheet(int chara, const uint8_t* rgba, float maxWidth)
{
	const Sheet* const sheet = SheetFor(chara);

	if (sheet == nullptr || sheet->state != SheetState::Ready)
		return false;

	IDirect3DTexture9* const texture = LargeFor(chara, rgba, sheet->large);

	if (texture == nullptr)
		return false;

	const float width = maxWidth;
	const float height = width * kLargeHeight / kLargeWidth;

	ImGui::Image(Handle(texture), ImVec2(width, height));

	return true;
}
