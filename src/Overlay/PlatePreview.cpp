#include "Overlay/PlatePreview.h"

#include "D3D9/DdsTexture.h"
#include "D3D9/DeviceHooks.h"
#include "Game/DataArchive.h"

#include <imgui.h>

#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

constexpr const char* kFolder = "plate";

constexpr float kArtWidth = 512.0f;
constexpr float kArtHeight = 192.0f;

constexpr PlayerCard::PlateLayer kDrawOrder[PlayerCard::kLayerCount] = {
	PlayerCard::PlateLayer::Base,
	PlayerCard::PlateLayer::Chara,
	PlayerCard::PlateLayer::Panel,
	PlayerCard::PlateLayer::Frame,
};

IDirect3DTexture9* LoadArt(const char* prefix, int id)
{
	IDirect3DDevice9* const device = DeviceHooks::GetDevice();
	if (device == nullptr)
		return nullptr;

	char file[32] = {};
	sprintf_s(file, "%s_%04d.dds", prefix, id);

	std::vector<uint8_t> data;
	if (!DataArchive::Read(kFolder, file, data))
		return nullptr;

	unsigned width = 0;
	unsigned height = 0;

	return DdsTexture::LoadFromMemory(device, data.data(), data.size(), file, width, height);
}

}

PlatePreview::~PlatePreview()
{
	Release();
}

void PlatePreview::Release()
{
	for (Layer& slot : m_layers)
	{
		if (slot.texture != nullptr)
			slot.texture->Release();

		slot.texture = nullptr;
		slot.id = -1;
		slot.resolved = false;
	}
}

void PlatePreview::Load(Layer& slot, PlayerCard::PlateLayer layer, int id)
{
	const char* const prefix = PlayerCard::LayerAssetPrefix(layer);

	slot.texture = LoadArt(prefix, id);

	if (slot.texture == nullptr && id != 0)
		slot.texture = LoadArt(prefix, 0);

	slot.id = id;
	slot.resolved = true;
}

bool PlatePreview::Ensure(PlayerCard::PlateLayer layer, int id)
{
	Layer& slot = m_layers[static_cast<int>(layer)];

	if (slot.resolved && slot.id == id)
		return slot.texture != nullptr;

	if (DeviceHooks::GetDevice() == nullptr)
		return false;

	if (slot.texture != nullptr)
	{
		slot.texture->Release();
		slot.texture = nullptr;
	}

	Load(slot, layer, id);
	return slot.texture != nullptr;
}

void PlatePreview::Draw(float width)
{
	if (width <= 0.0f)
		width = kArtWidth;

	const float height = width * kArtHeight / kArtWidth;

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 corner = ImVec2(origin.x + width, origin.y + height);

	ImDrawList* const draw = ImGui::GetWindowDrawList();

	draw->AddRectFilled(origin, corner, IM_COL32(24, 24, 28, 255));

	int drawn = 0;

	for (const PlayerCard::PlateLayer layer : kDrawOrder)
	{
		int id = 0;
		if (!PlayerCard::GetPlate(layer, id))
			continue;

		if (!Ensure(layer, id))
			continue;

		const ImTextureID handle =
			static_cast<ImTextureID>(reinterpret_cast<intptr_t>(m_layers[static_cast<int>(layer)].texture));

		draw->AddImage(handle, origin, corner);
		++drawn;
	}

	draw->AddRect(origin, corner, IM_COL32(120, 120, 130, 255));

	ImGui::Dummy(ImVec2(width, height));

	if (drawn > 0)
		return;

	ImGui::TextDisabled("The plate art could not be read out of the game's archive.");
}
