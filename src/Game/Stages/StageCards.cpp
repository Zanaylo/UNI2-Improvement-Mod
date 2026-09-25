#include "Game/Stages/StageCards.h"

#include "Game/Stages/BgMipmaps.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/BgListOverride.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/SceneWatch.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Stages/StageThumb.h"
#include "Hooks/GameHook.h"
#include "Game/Engine/CodeSignatures.h"

#include <Windows.h>
#include <d3d9.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr int kCardWidth = 120;
constexpr int kCardHeight = 320;
constexpr int kColumns = 8;
constexpr int kFirstX = 4;
constexpr int kFirstY = 2;
constexpr int kPitchX = 128;
constexpr int kPitchY = 336;
constexpr int kSecondSheet = 24;
constexpr int kWindow = 9;
constexpr int kPerFrame = 4;
constexpr size_t kCachedCards = 24;
constexpr const char* kSheetFile = "Mods\\grpdat\\CSel\\stage_thumb01.dds";

using Setup_t = int(__fastcall*)(void*, void*);

using CreateTexture_t = HRESULT(WINAPI*)(void*, const void*, UINT, UINT, UINT, UINT, DWORD, DWORD,
	DWORD, DWORD, DWORD, DWORD, void*, void*, IDirect3DTexture9**);

GameHook<Setup_t> g_setupHook("StageSelectSetup");
GameHook<CreateTexture_t> g_d3DXCreateTextureFromFileInMemoryExHook("D3DXCreateTextureFromFileInMemoryEx");

void* volatile g_picker = nullptr;
IDirect3DTexture9* volatile g_sheet = nullptr;

std::vector<uint8_t> g_wanted;
std::vector<int> g_order;
std::map<int, std::vector<uint8_t> > g_cards;
int g_holds[StageThumb::kCells] = {};
int g_cursor = -1;
bool g_ordered = false;
bool g_pending = false;

char g_status[224] = "the picker's cards are the game's own";

bool ReadWhole(const std::string& path, std::vector<uint8_t>& out)
{
	out.clear();

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	fseek(handle, 0, SEEK_END);
	const long bytes = ftell(handle);
	fseek(handle, 0, SEEK_SET);

	if (bytes <= 0)
	{
		fclose(handle);
		return false;
	}

	out.resize(static_cast<size_t>(bytes));
	const size_t read = fread(out.data(), 1, out.size(), handle);
	fclose(handle);

	if (read == out.size())
		return true;

	out.clear();
	return false;
}

int __fastcall HookedSetup(void* self, void* unused)
{
	ReadWhole(GetModRootPath(kSheetFile), g_wanted);
	g_cards.clear();

	const int result = g_setupHook.Original()(self, unused);

	g_picker = self;
	g_cursor = -1;
	g_pending = false;
	g_ordered = false;

	for (int& holds : g_holds)
		holds = 0;

	return result;
}

HRESULT WINAPI HookedCreateTexture(void* device, const void* source, UINT bytes, UINT width,
	UINT height, UINT levels, DWORD usage, DWORD format, DWORD pool, DWORD filter,
	DWORD mipFilter, DWORD colourKey, void* info, void* palette, IDirect3DTexture9** texture)
{
	UINT wantedLevels = levels;
	const bool baked = BgMipmaps::FromFile(source, bytes, wantedLevels);

	const DWORD began = GetTickCount();

	HRESULT result = g_d3DXCreateTextureFromFileInMemoryExHook.Original()(device, source, bytes, width, height, wantedLevels, usage,
		format, pool, filter, mipFilter, colourKey, info, palette, texture);

	BgMipmaps::Note(GetTickCount() - began, baked);

	if (FAILED(result) && baked)
	{
		result = g_d3DXCreateTextureFromFileInMemoryExHook.Original()(device, source, bytes, width, height, levels, usage, format, pool,
			filter, mipFilter, colourKey, info, palette, texture);
	}

	if (FAILED(result) || texture == nullptr || *texture == nullptr)
		return result;

	if (g_wanted.empty() || bytes != g_wanted.size()
		|| memcmp(source, g_wanted.data(), g_wanted.size()) != 0)
	{
		return result;
	}

	D3DSURFACE_DESC desc = {};

	if (FAILED((*texture)->GetLevelDesc(0, &desc)))
		return result;

	if (desc.Pool == D3DPOOL_DEFAULT)
	{
		sprintf_s(g_status, "the picker's sheet is in video memory, so the mod cannot paint it");
		LOG("StageCards: %s", g_status);
		return result;
	}

	if (desc.Format != D3DFMT_A8R8G8B8 && desc.Format != D3DFMT_X8R8G8B8)
	{
		sprintf_s(g_status, "the picker's sheet came out as format %u, which the mod cannot paint",
			static_cast<unsigned>(desc.Format));
		LOG("StageCards: %s", g_status);
		return result;
	}

	IDirect3DTexture9* const was = g_sheet;

	(*texture)->AddRef();
	g_sheet = *texture;

	if (was != nullptr)
		was->Release();

	for (int& holds : g_holds)
		holds = 0;

	sprintf_s(g_status, "painting the picker's cards, sheet %ux%u", desc.Width, desc.Height);
	LOG("StageCards: %s", g_status);

	return result;
}

void CellOrigin(int cell, int& outX, int& outY)
{
	const int local = cell - kSecondSheet;

	outX = kFirstX + (local % kColumns) * kPitchX;
	outY = kFirstY + (local / kColumns) * kPitchY;
}

const std::vector<uint8_t>& CardOf(int id)
{
	static const std::vector<uint8_t> none;

	const std::map<int, std::vector<uint8_t> >::const_iterator known = g_cards.find(id);

	if (known != g_cards.end())
		return known->second;

	if (g_cards.size() >= kCachedCards)
		g_cards.clear();

	std::vector<uint8_t> card;

	if (!ReadWhole(StageThumb::CardPath(id), card))
		return none;

	return g_cards.insert(std::make_pair(id, card)).first->second;
}

bool PaintCell(int cell, int id)
{
	IDirect3DTexture9* const sheet = g_sheet;

	if (sheet == nullptr)
		return false;

	const std::vector<uint8_t>& card = CardOf(id);

	if (card.size() != static_cast<size_t>(kCardWidth) * kCardHeight * 4)
		return false;

	int x = 0;
	int y = 0;
	CellOrigin(cell, x, y);

	RECT area = { x, y, x + kCardWidth, y + kCardHeight };
	D3DLOCKED_RECT locked = {};

	if (FAILED(sheet->LockRect(0, &locked, &area, 0)))
		return false;

	for (int row = 0; row < kCardHeight; ++row)
	{
		memcpy(static_cast<uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
			&card[static_cast<size_t>(row) * kCardWidth * 4],
			static_cast<size_t>(kCardWidth) * 4);
	}

	sheet->UnlockRect(0);
	return true;
}

bool Refresh(int cursor)
{
	if (!g_ordered)
	{
		BgListOverride::SelectOrder(g_order);
		g_ordered = true;
	}

	const int count = static_cast<int>(g_order.size());
	int painted = 0;

	if (count <= 0)
		return true;

	for (int step = 0; step <= kWindow * 2; ++step)
	{
		const int away = (step + 1) / 2;

		if (away > kWindow || away * 2 > count)
			continue;

		const int at = ((step % 2 == 0 ? cursor - away : cursor + away) % count + count) % count;

		const int cell = StageThumb::CellFor(g_order[at]);

		if (cell < StageThumb::kFirstCell || cell > StageThumb::kLastCell)
			continue;

		const int id = StageLibrary::IdForSlot(g_order[at]);
		const int hold = cell - StageThumb::kFirstCell;

		if (id < 0 || g_holds[hold] == id)
			continue;

		if (!StageThumb::HasCard(id) || !PaintCell(cell, id))
			continue;

		g_holds[hold] = id;

		if (++painted >= kPerFrame)
			return false;
	}

	return true;
}

}

bool StageCards::Initialize()
{
	StageThumb::ServeSheet();
	ReadWhole(GetModRootPath(kSheetFile), g_wanted);

	const uintptr_t address = CodeSignatures::Address(GameOffsets::kFnStageSelectSetup);

	if (!IsAddressInGameModule(address))
	{
		strncpy_s(g_status, "the stage picker's setup was not where it was left", _TRUNCATE);
		LOG("StageCards: %s", g_status);
		return false;
	}

	if (!g_setupHook.Install(reinterpret_cast<void*>(address), &HookedSetup))
	{
		strncpy_s(g_status, "the stage picker's setup could not be hooked", _TRUNCATE);
		return false;
	}

	if (!g_d3DXCreateTextureFromFileInMemoryExHook.InstallApi("d3dx9_42.dll", "D3DXCreateTextureFromFileInMemoryEx", &HookedCreateTexture))
	{
		strncpy_s(g_status, "the texture loader could not be hooked", _TRUNCATE);
		LOG("StageCards: %s", g_status);
		return false;
	}

	return true;
}

void StageCards::OnFrame()
{
	void* const picker = g_picker;

	if (picker == nullptr || g_sheet == nullptr)
		return;

	if (SceneWatch::Current() != GameOffsets::kSceneStageSelect)
		return;

	int cursor = 0;

	if (!TryReadMemory(&cursor, static_cast<const uint8_t*>(picker) +
		GameOffsets::kStageSelectCursor, sizeof(cursor)))
	{
		return;
	}

	if (cursor < 0 || cursor > 4096)
		return;

	if (cursor != g_cursor)
	{
		g_cursor = cursor;
		g_pending = true;
	}

	if (g_pending)
		g_pending = !Refresh(g_cursor);
}

void StageCards::Repaint()
{
	for (int& holds : g_holds)
		holds = 0;

	g_cards.clear();
	g_ordered = false;
	g_pending = true;
}

bool StageCards::Reached()
{
	return g_sheet != nullptr;
}

const char* StageCards::StatusText()
{
	return g_status;
}
