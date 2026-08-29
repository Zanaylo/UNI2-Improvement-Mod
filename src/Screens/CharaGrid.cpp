#include "Screens/CharaGrid.h"

#include "Core/logger.h"
#include "Game/DataArchive.h"
#include "Screens/PatTextures.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kDocument = "game:grpdat/CSel/csel00.pat";
constexpr int kIconFrames = 4;
constexpr int kCursorFrames = 2;
constexpr int kCursorLayers = 2;

struct Slot
{
	int frames[kIconFrames];
	int themePart;
	int gamePart;
	float gameZoom;
	int cursorParts[kCursorLayers];
	float x;
	float y;
	int id;
};

std::vector<Slot> g_slots;
int g_tagParts[CharaGrid::kSideCount] = { -1, -1 };
CharaGrid::Style g_style = {};
int g_cursor[CharaGrid::kSideCount] = { CharaGrid::kNoSlot, CharaGrid::kNoSlot };
PatFile::Handle g_pat = PatFile::kInvalid;
bool g_tried = false;
bool g_styled = false;
char g_status[192] = "not loaded";

std::string Trim(const std::string& text)
{
	size_t first = 0;

	while (first < text.size() && (text[first] == ' ' || text[first] == '\t'))
		++first;

	size_t last = text.size();

	while (last > first)
	{
		const char c = text[last - 1];

		if (c != ' ' && c != '\t' && c != '\r' && c != ';')
			break;

		--last;
	}

	return text.substr(first, last - first);
}

void ResolveIcons(const std::string& icon, Slot& slot)
{
	for (int i = 0; i < kIconFrames; ++i)
	{
		char name[64] = {};
		sprintf_s(name, "%s_%03d", icon.c_str(), i);
		slot.frames[i] = PatFile::FindPattern(g_pat, name);
	}
}

bool ParseLayout(const std::vector<uint8_t>& text)
{
	g_slots.clear();

	std::string icon;
	Slot slot = {};
	bool open = false;
	size_t at = 0;

	const std::string body(reinterpret_cast<const char*>(text.data()), text.size());

	while (at <= body.size())
	{
		size_t end = body.find('\n', at);

		if (end == std::string::npos)
			end = body.size();

		std::string line = body.substr(at, end - at);
		at = end + 1;

		const size_t comment = line.find("//");

		if (comment != std::string::npos)
			line.resize(comment);

		line = Trim(line);

		if (line.empty())
			continue;

		if (line[0] == '[')
		{
			if (open && !icon.empty())
			{
				ResolveIcons(icon, slot);
				g_slots.push_back(slot);
			}

			slot = Slot();
			slot.themePart = -1;
			slot.gamePart = -1;
			slot.gameZoom = 1.0f;
			slot.cursorParts[0] = -1;
			slot.cursorParts[1] = -1;
			slot.id = atoi(line.c_str() + 1);
			icon.clear();
			open = true;
			continue;
		}

		const size_t split = line.find('=');

		if (split == std::string::npos || !open)
			continue;

		const std::string key = Trim(line.substr(0, split));
		const std::string value = Trim(line.substr(split + 1));

		if (_stricmp(key.c_str(), "icon") == 0)
			icon = value;
		else if (_stricmp(key.c_str(), "position_x") == 0)
			slot.x = static_cast<float>(atof(value.c_str()));
		else if (_stricmp(key.c_str(), "position_y") == 0)
			slot.y = static_cast<float>(atof(value.c_str()));
	}

	if (open && !icon.empty())
	{
		ResolveIcons(icon, slot);
		g_slots.push_back(slot);
	}

	return !g_slots.empty();
}

int BiggestPartOf(PatFile::Handle pat, int pattern)
{
	const int count = PatFile::SpriteCount(pat, pattern);
	const PatFile::Sprite* const sprites = PatFile::GetSprites(pat, pattern);

	if (count <= 0 || sprites == nullptr)
		return -1;

	int best = -1;
	int bestArea = 0;

	for (int i = 0; i < count; ++i)
	{
		PatFile::Part part = {};

		if (!PatFile::GetPart(pat, sprites[i].part, part))
			continue;

		const int area = part.width * part.height;

		if (area <= bestArea)
			continue;

		bestArea = area;
		best = sprites[i].part;
	}

	return best;
}

void ResolveStyle()
{
	g_styled = true;

	int dressed = 0;
	int cardHeight = 0;

	for (Slot& slot : g_slots)
	{
		slot.themePart = -1;
		slot.cursorParts[0] = -1;
		slot.cursorParts[1] = -1;

		if (g_style.pat == PatFile::kInvalid || g_style.codes == nullptr)
			continue;

		if (slot.id < 0 || slot.id >= static_cast<int>(g_style.codes->size()))
			continue;

		const std::string& code = (*g_style.codes)[static_cast<size_t>(slot.id)];

		if (code.empty())
			continue;

		slot.themePart = PatFile::FindPartBySuffix(g_style.pat, code.c_str());

		if (slot.themePart < 0)
			continue;

		++dressed;

		if (cardHeight == 0)
		{
			PatFile::Part card = {};

			if (PatFile::GetPart(g_style.pat, slot.themePart, card))
				cardHeight = card.height;
		}

		if (g_style.cursor.empty())
			continue;

		for (int layer = 0; layer < kCursorLayers; ++layer)
		{
			char name[40] = {};
			sprintf_s(name, "%s%02d%d", g_style.cursor.c_str(), slot.id, layer);
			slot.cursorParts[layer] = PatFile::FindPartBySuffix(g_style.pat, name);
		}
	}

	for (Slot& slot : g_slots)
	{
		slot.gamePart = -1;
		slot.gameZoom = 1.0f;

		if (slot.themePart >= 0 || slot.frames[kIconFrames - 1] < 0)
			continue;

		slot.gamePart = BiggestPartOf(g_pat, slot.frames[kIconFrames - 1]);

		if (slot.gamePart < 0 || cardHeight <= 0)
			continue;

		PatFile::Part own = {};

		if (PatFile::GetPart(g_pat, slot.gamePart, own) && own.height > 0)
			slot.gameZoom = static_cast<float>(cardHeight) / own.height;
	}

	for (int side = 0; side < CharaGrid::kSideCount; ++side)
	{
		g_tagParts[side] = g_style.tag[side].empty()
			? -1
			: PatFile::FindPartBySuffix(g_style.pat, g_style.tag[side].c_str());
	}

	sprintf_s(g_status, "%d slot(s), %d wearing the theme's own card, card height %d",
		static_cast<int>(g_slots.size()), dressed, cardHeight);

	LOG("CharaGrid: %s", g_status);
}

}

bool CharaGrid::Prepare(IDirect3DDevice9* device)
{
	if (device == nullptr)
		return false;

	if (!g_slots.empty() && g_pat != PatFile::kInvalid)
		return true;

	if (g_tried)
		return false;

	g_tried = true;

	if (!DataArchive::IsAvailable())
	{
		strncpy_s(g_status, "the game's own archive could not be read", _TRUNCATE);
		LOG("CharaGrid: %s", g_status);
		return false;
	}

	std::vector<uint8_t> art;

	if (!DataArchive::Read("CSel", "csel00.pat", art))
	{
		strncpy_s(g_status, "the game has no grpdat/CSel/csel00.pat", _TRUNCATE);
		LOG("CharaGrid: %s", g_status);
		return false;
	}

	g_pat = PatFile::LoadFromMemory(kDocument, art.data(), art.size());

	if (g_pat == PatFile::kInvalid)
	{
		sprintf_s(g_status, "%s", PatFile::StatusText());
		LOG("CharaGrid: %s", g_status);
		return false;
	}

	if (!PatTextures::Prepare(device, g_pat))
	{
		strncpy_s(g_status, "no atlas of the game's csel00.pat became a texture", _TRUNCATE);
		LOG("CharaGrid: %s", g_status);
		return false;
	}

	std::vector<uint8_t> layout;

	if (!DataArchive::Read("CSel", "CSelAnim.ini", layout) || !ParseLayout(layout))
	{
		strncpy_s(g_status, "the game has no readable CSelAnim.ini", _TRUNCATE);
		LOG("CharaGrid: %s", g_status);
		return false;
	}

	g_styled = false;
	return true;
}

void CharaGrid::SetStyle(const Style& style)
{
	g_style = style;
	g_styled = false;
}

int CharaGrid::Draw(const PatPainter::Placement& where, unsigned elapsedMs)
{
	if (g_pat == PatFile::kInvalid || g_slots.empty())
		return 0;

	if (!g_styled)
		ResolveStyle();

	const float zoom = g_style.zoom > 0.0f ? g_style.zoom : 1.0f;
	const float spreadX = g_style.spreadX > 0.0f ? g_style.spreadX : 1.0f;
	const float spreadY = g_style.spreadY > 0.0f ? g_style.spreadY : 1.0f;

	int drawn = 0;

	for (const Slot& slot : g_slots)
	{
		const float x = slot.x * spreadX;
		const float y = slot.y * spreadY;

		if (slot.themePart >= 0)
		{
			if (PatPainter::DrawPart(g_style.pat, slot.themePart, where, x, y, zoom, 0xFFFFFFFF))
				++drawn;

			continue;
		}

		if (slot.gamePart < 0)
			continue;

		if (PatPainter::DrawPart(g_pat, slot.gamePart, where, x, y, zoom * slot.gameZoom,
			0xFFFFFFFF))
		{
			++drawn;
		}
	}

	if (g_style.pat == PatFile::kInvalid || g_style.cursor.empty())
		return drawn;

	const int fps = g_style.fps > 0 ? g_style.fps : 20;
	const int frame = static_cast<int>(elapsedMs * static_cast<unsigned>(fps) / 1000u) %
		kCursorFrames;

	for (int side = 0; side < kSideCount; ++side)
	{
		const int index = g_cursor[side];

		if (index < 0 || index >= static_cast<int>(g_slots.size()))
			continue;

		const Slot& slot = g_slots[index];
		const float x = slot.x * spreadX;
		const float y = slot.y * spreadY;

		for (int layer = 0; layer < kCursorLayers; ++layer)
		{
			if (slot.cursorParts[layer] < 0)
				continue;

			if (PatPainter::DrawPart(g_style.pat, slot.cursorParts[layer], where, x, y, zoom,
				g_style.tint[side][frame]))
			{
				++drawn;
			}
		}

		if (g_tagParts[side] < 0)
			continue;

		if (PatPainter::DrawPart(g_style.pat, g_tagParts[side], where,
			x + g_style.tagX[side] * zoom, y + g_style.tagY[side] * zoom, zoom, 0xFFFFFFFF))
		{
			++drawn;
		}
	}

	return drawn;
}

void CharaGrid::Invalidate()
{
	g_slots.clear();
	g_pat = PatFile::kInvalid;
	g_tried = false;
	g_styled = false;
	strncpy_s(g_status, "not loaded", _TRUNCATE);
}

int CharaGrid::SlotCount()
{
	return static_cast<int>(g_slots.size());
}

void CharaGrid::SetCursorFromId(int side, uint32_t id)
{
	for (size_t i = 0; i < g_slots.size(); ++i)
	{
		if (static_cast<uint32_t>(g_slots[i].id) != id)
			continue;

		SetCursor(side, static_cast<int>(i));
		return;
	}

	SetCursor(side, kNoSlot);
}

void CharaGrid::SetCursor(int side, int slot)
{
	if (side < 0 || side >= kSideCount)
		return;

	g_cursor[side] = slot;
}

int CharaGrid::GetCursor(int side)
{
	return side >= 0 && side < kSideCount ? g_cursor[side] : kNoSlot;
}

const char* CharaGrid::StatusText()
{
	return g_status;
}
