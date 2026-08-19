#include "Overlay/Window/HitboxOverlay.h"

#include "Core/interfaces.h"
#include "Game/Camera.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/HitboxData.h"
#include "Game/MemoryMap.h"
#include "Game/OnlineState.h"
#include "Game/PlayerState.h"
#include "Training/FrameStepper.h"

#include <algorithm>

namespace {

constexpr int kMaxEffects = 64;
constexpr int kMaxEntities = GameOffsets::kCharaArrayCount + kMaxEffects;

void* g_entities[kMaxEntities] = {};
int g_firstEffect = 0;

int EnumerateEntities(void**& outEntities, int& outFirstEffect)
{
	int count = MemoryMap::EnumerateCharaSlots(g_entities, GameOffsets::kCharaArrayCount, true);

	g_firstEffect = count;
	count += MemoryMap::EnumerateEffectSlotsCached(g_entities + count, kMaxEntities - count);

	outEntities = g_entities;
	outFirstEffect = g_firstEffect;
	return count;
}

bool HasInteractiveBox(const HitboxData::Box* boxes, int count)
{
	for (int i = 0; i < count; ++i)
	{
		if (boxes[i].arrayIndex == HitboxData::BoxArray_Attack)
			return true;

		if (boxes[i].arrayIndex != HitboxData::BoxArray_Normal)
			continue;

		if (boxes[i].index >= 1 && boxes[i].index <= 8)
			return true;

		if (boxes[i].index == 11 || boxes[i].index == 12)
			return true;
	}

	return false;
}

struct CategoryInfo
{
	const char* name;
	unsigned int color;
	const char* summary;
	const char* detail;
};

const CategoryInfo kCategories[HitboxOverlay::BoxCategory_COUNT] = {
	{
		"Pushbox", IM_COL32(255, 255, 255, 255),
		"Body collision. Two characters cannot walk through each other.",
		"Never hits and is never hit."
	},
	{
		"Hurtbox", IM_COL32(60, 220, 60, 255),
		"Where the character can be hit.",
		"A hit happens when a hitbox overlaps one, so a move with none of these is strike "
		"invulnerable.\n\nThrows ignore hurtboxes, so a move that cannot be hit can still be thrown."
	},
	{
		"Hitbox", IM_COL32(255, 60, 60, 255),
		"The attack itself.",
		"Out only on the frames the attack can connect.\n\nA throw's grab range is one of these too - there "
		"is no separate throw box."
	},
	{
		"Clash", IM_COL32(60, 220, 230, 255),
		"Trades with an attack instead of being hit by it.",
		"Both characters freeze, the attacker's move counts as a whiff, and you are strike invulnerable "
		"while it is out.\n\nEveryone has one on the Creeping Edge. Yuzuriha's 4B is the only normal with "
		"one."
	},
	{
		"Projectile clash", IM_COL32(240, 150, 50, 255),
		"Destroys or trades with projectiles.",
		"Trades with an attack box. Two of these touching each other does nothing.\n\nOn moves made to swat "
		"projectiles, and on setups that can be destroyed."
	},
	{
		"Guard point", IM_COL32(170, 90, 230, 255),
		"Absorbs an attack: no damage, no blockstun.",
		"Whatever lands on it is swallowed. What happens after is up to the move.\n\nThe D Shield is not "
		"this. It has no box, so nothing is drawn for it."
	},
	{
		"Crouch block check", IM_COL32(120, 120, 240, 255),
		"The crouching silhouette, on someone blocking while standing.",
		"A jump attack that misses it can also be blocked crouching.\n\nOnly a jump attack out of Nanase's "
		"or Uduki's air B+C can trigger that. On everyone else nothing reads this box."
	},
	{
		"Pull box", IM_COL32(90, 200, 130, 255),
		"Drags the opponent towards you.",
		"Pulls them in while it overlaps their hurtbox. Nanase's 623 and the moves that copy it.\n\nOn a "
		"chip-death knockdown it does not pull - it is where Phonon's throw rope attaches."
	},
	{
		"Marker", IM_COL32(150, 150, 160, 255),
		"A point, not a box. Only its corner matters.",
		"Marks where an effect hangs or where a throw is drawn from.\n\nKaguya's and Linne's B+C watch it to "
		"spot an attack passing through them, which plays the dodge flash. The dodge itself is "
		"invulnerability."
	},
	{
		"Grab point", IM_COL32(230, 110, 180, 255),
		"Where a throw grabs you: head, neck, belly, leg.",
		"A throw takes the first one you have, so it lines up on any height.\n\nNothing collides with them."
	},
	{
		"Inert", IM_COL32(140, 110, 150, 255),
		"Looks like a catch box, catches nothing.",
		"It sits on an index used for Clash and Guard point, but no move armed it, so attacks pass straight "
		"through.\n\nMerkava's flight is the case: five boxes out, none of them live."
	},
	{
		"Other", IM_COL32(230, 230, 60, 255),
		"Boxes with no shared meaning.",
		"Mostly effect anchors - Wagner's shield and sword, Eltnum's lasers.\n\nThe same number means "
		"different things on different characters."
	},
};

constexpr int kBoxClash = 11;
constexpr int kBoxProjectileClash = 12;
constexpr int kBoxPull = 13;
constexpr int kBoxGuardPoint = 14;
constexpr int kBoxCrouchBlock = 24;

bool IsMarkerBox(int index)
{
	return index == 9 || index == 10;
}

bool IsGrabPointBox(int index)
{
	return index >= 18 && index <= 21;
}

unsigned int WithAlpha(unsigned int color, float alpha)
{
	const unsigned int a = static_cast<unsigned int>(alpha * 255.0f) & 0xff;
	return (color & 0x00ffffff) | (a << 24);
}

}

HitboxOverlay::HitboxOverlay(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
	, m_showOrigin(true)
{
	for (int i = 0; i < BoxCategory_COUNT; ++i)
	{
		m_categories[i].enabled = true;
		m_categories[i].fillAlpha = 0.22f;
		m_categories[i].outlineAlpha = 0.9f;
	}

	m_categories[BoxCategory_Pushbox].fillAlpha = 0.0f;

	m_categories[BoxCategory_CrouchBlock].enabled = false;
	m_categories[BoxCategory_Pull].enabled = false;
	m_categories[BoxCategory_Marker].enabled = false;
	m_categories[BoxCategory_GrabPoint].enabled = false;
	m_categories[BoxCategory_Inert].enabled = false;
	m_categories[BoxCategory_Other].enabled = false;
}

const char* HitboxOverlay::GetCategoryName(int category)
{
	return kCategories[category].name;
}

const char* HitboxOverlay::GetCategorySummary(int category)
{
	return kCategories[category].summary;
}

const char* HitboxOverlay::GetCategoryDetail(int category)
{
	return kCategories[category].detail;
}

unsigned int HitboxOverlay::GetCategoryColor(int category)
{
	return kCategories[category].color;
}

int HitboxOverlay::ClassifyBox(const HitboxData::Box& box, int catchBoxIndex)
{
	if (box.arrayIndex == HitboxData::BoxArray_Attack)
		return BoxCategory_Hitbox;

	if (box.index == 0)
		return BoxCategory_Pushbox;

	if (box.index <= 8)
		return BoxCategory_Hurtbox;

	if (catchBoxIndex >= 0 && box.index == catchBoxIndex)
		return box.index == kBoxClash ? BoxCategory_Clash : BoxCategory_GuardPoint;

	if (box.index == kBoxProjectileClash)
		return BoxCategory_ProjectileClash;

	if (box.index == kBoxPull)
		return BoxCategory_Pull;

	if (box.index == kBoxCrouchBlock)
		return BoxCategory_CrouchBlock;

	if (IsMarkerBox(box.index))
		return BoxCategory_Marker;

	if (IsGrabPointBox(box.index))
		return BoxCategory_GrabPoint;

	if (box.index == kBoxClash || box.index == kBoxGuardPoint)
		return BoxCategory_Inert;

	return BoxCategory_Other;
}

void HitboxOverlay::BeforeDraw()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
}

void HitboxOverlay::AfterDraw()
{
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
}

void HitboxOverlay::Draw()
{

	if (!GameState::IsInMatch() || OnlineState::IsOnline() || !Camera::IsAvailable())
		return;

	if (!GameState::IsBattleTicking() && !FrameStepper::IsPaused() && !g_modVals.drawWhilePaused)
		return;

	void** entities = nullptr;
	int firstEffect = 0;
	const int entityCount = EnumerateEntities(entities, firstEffect);

	for (int i = 0; i < entityCount; ++i)
		DrawEntity(entities[i], i >= firstEffect);
}

void HitboxOverlay::DrawEntity(void* entity, bool isEffect)
{
	HitboxData::FrameObject frame = {};
	if (!HitboxData::Resolve(entity, frame))
		return;

	HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
	const int count = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);
	if (count == 0)
		return;

	if (isEffect && !HasInteractiveBox(boxes, count))
		return;

	int worldX = 0;
	int worldY = 0;
	if (!Camera::GetWorldPosition(entity, worldX, worldY))
		return;

	float positionScale = 0.0f;
	if (!Camera::GetPositionScale(positionScale))
		return;

	const float originPixelX = worldX * positionScale;
	const float originPixelY = worldY * positionScale;
	const float facing = static_cast<float>(Camera::GetFacing(entity));

	const int catchBoxIndex = isEffect ? -1 : PlayerState::ReadCatchBoxIndex(entity);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	if (m_showOrigin)
	{
		float originX = 0.0f;
		float originY = 0.0f;
		if (Camera::PixelToScreen(originPixelX, originPixelY, originX, originY))
		{
			drawList->AddLine(ImVec2(originX - 8.0f, originY), ImVec2(originX + 8.0f, originY),
				IM_COL32(255, 255, 0, 200), 1.5f);
			drawList->AddLine(ImVec2(originX, originY - 8.0f), ImVec2(originX, originY + 8.0f),
				IM_COL32(255, 255, 0, 200), 1.5f);
		}
	}

	for (int i = 0; i < count; ++i)
	{
		const HitboxData::Box& box = boxes[i];
		const int category = ClassifyBox(box, catchBoxIndex);
		const CategorySettings& settings = m_categories[category];

		if (!settings.enabled)
			continue;

		const float left = originPixelX + box.x1 * facing;
		const float right = originPixelX + box.x2 * facing;
		const float top = originPixelY + box.y1;
		const float bottom = originPixelY + box.y2;

		float screenLeft = 0.0f;
		float screenTop = 0.0f;
		float screenRight = 0.0f;
		float screenBottom = 0.0f;

		if (!Camera::PixelToScreen(left, top, screenLeft, screenTop) ||
			!Camera::PixelToScreen(right, bottom, screenRight, screenBottom))
		{
			continue;
		}

		if (screenLeft > screenRight)
			std::swap(screenLeft, screenRight);
		if (screenTop > screenBottom)
			std::swap(screenTop, screenBottom);

		const ImVec2 topLeft(screenLeft, screenTop);
		const ImVec2 bottomRight(screenRight, screenBottom);
		const unsigned int color = GetCategoryColor(category);

		if (settings.fillAlpha > 0.0f)
			drawList->AddRectFilled(topLeft, bottomRight, WithAlpha(color, settings.fillAlpha));

		if (settings.outlineAlpha > 0.0f)
			drawList->AddRect(topLeft, bottomRight, WithAlpha(color, settings.outlineAlpha), 0.0f, 1.5f);
	}
}
