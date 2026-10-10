#include "Game/Display/InputHistoryView.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "D3D9/Draw/DrawQueue.h"
#include "D3D9/Draw/QueuedItemScale.h"
#include "Game/Display/InputHistoryLog.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Hooks/GameHook.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>

namespace {

typedef void(__fastcall* DrawFn)(void* block, void* unused);

constexpr const char* kSection = "Training";
constexpr uint32_t kBehindCharacters = 340;
constexpr int kSides = 2;
constexpr int kNewestSlot = InputHistoryLog::kRing - 1;
constexpr float kLeverWidth = 25.0f;
constexpr int kCountsLeft = 0x59;
constexpr int kDigitStep = 0xc;
constexpr int kDigitWidth = 0x10;
constexpr int kNarrowDigits = 2;
constexpr int kWideDigits = 4;

struct Scope
{
	bool active;
	bool anchored;
	bool rightSide;
	int digits;
	float scale;
	float anchorX;
	float anchorY;
};

GameHook<DrawFn> g_drawHook("InputHistoryDraw");
InputHistoryLog g_logs[kSides];
Scope g_scope = {};
bool g_installed = false;

template <typename T>
T& Field(void* block, uintptr_t offset)
{
	return *reinterpret_cast<T*>(static_cast<uint8_t*>(block) + offset);
}

InputHistoryLog::Entry (&Ring(void* block))[InputHistoryLog::kRing]
{
	return *reinterpret_cast<InputHistoryLog::Entry(*)[InputHistoryLog::kRing]>(
		static_cast<uint8_t*>(block) + GameOffsets::kInputHistoryEntries);
}

bool Changes()
{
	return InputHistoryView::IsBehind() || InputHistoryView::Rows() > InputHistoryView::kGameRows;
}

float PanelWidth(int digits)
{
	return static_cast<float>(kCountsLeft + (digits - 1) * kDigitStep + kDigitWidth);
}

void Anchor(const void* command)
{
	float left = 0.0f;
	float top = 0.0f;
	float width = 0.0f;

	if (!QueuedItemScale::Corner(command, left, top, width))
		return;

	const float units = width / kLeverWidth;

	g_scope.anchorX = g_scope.rightSide ? left + PanelWidth(g_scope.digits) * units : left;
	g_scope.anchorY = top;
	g_scope.anchored = true;
}

class Placer : public DrawQueue::IObserver
{
public:
	uint32_t OnPush(uint32_t layer, void* command) override
	{
		if (!g_scope.active)
			return layer;

		if (!g_scope.anchored)
			Anchor(command);

		QueuedItemScale::Apply(command, g_scope.anchorX, g_scope.anchorY, g_scope.scale);

		return InputHistoryView::IsBehind() ? kBehindCharacters : layer;
	}
};

Placer g_placer;

void FillOlder(uint8_t* copy, const InputHistoryLog& log, int first, int count, int top)
{
	InputHistoryLog::Entry (&ring)[InputHistoryLog::kRing] = Ring(copy);
	memset(ring, 0, sizeof(ring));

	for (int i = 0; i < count; ++i)
		ring[kNewestSlot - i] = log.Older(first + i);

	Field<int>(copy, GameOffsets::kInputHistoryHead) = kNewestSlot;
	Field<int>(copy, GameOffsets::kInputHistoryRows) = count;
	Field<int>(copy, GameOffsets::kInputHistoryTop) = top;
}

void DrawOlder(void* block, const InputHistoryLog& log)
{
	const int extra = InputHistoryView::Rows() - InputHistoryView::kGameRows;
	const int available = extra < log.OlderCount() ? extra : log.OlderCount();
	const int top = Field<int>(block, GameOffsets::kInputHistoryTop);

	uint8_t copy[GameOffsets::kInputHistoryBlockSize] = {};

	for (int first = 0; first < available; first += InputHistoryLog::kRing)
	{
		const int left = available - first;
		const int count = left < InputHistoryLog::kRing ? left : InputHistoryLog::kRing;
		const int rows = InputHistoryView::kGameRows + first;

		memcpy(copy, block, sizeof(copy));
		FillOlder(copy, log, first, count, top + rows * GameOffsets::kInputHistoryRowPitch);
		g_drawHook.Original()(copy, nullptr);
	}
}

void Open(void* block)
{
	const bool rightSide = Field<int>(block, GameOffsets::kInputHistoryRightSide) != 0;

	g_scope = {};
	g_scope.active = true;
	g_scope.rightSide = rightSide;
	g_scope.digits = Field<int>(block, GameOffsets::kInputHistoryWideCounts) != 0 ? kWideDigits : kNarrowDigits;
	g_scope.scale = static_cast<float>(InputHistoryView::kGameRows) / InputHistoryView::Rows();
}

void __fastcall HookedDraw(void* block, void* unused)
{
	if (!Changes())
	{
		g_drawHook.Original()(block, unused);
		return;
	}

	InputHistoryLog& log = g_logs[Field<int>(block, GameOffsets::kInputHistoryRightSide) != 0 ? 1 : 0];
	log.Observe(Ring(block), Field<int>(block, GameOffsets::kInputHistoryHead), GetTickCount());

	Open(block);
	g_drawHook.Original()(block, unused);
	DrawOlder(block, log);
	g_scope.active = false;
}

int Bounded(int rows)
{
	if (rows < InputHistoryView::kGameRows)
		return InputHistoryView::kGameRows;

	return rows > InputHistoryView::kMostRows ? InputHistoryView::kMostRows : rows;
}

}

bool InputHistoryView::Install()
{
	if (!DrawQueue::Install() || !DrawQueue::AddObserver(&g_placer))
	{
		LOG("InputHistoryView: the draw queue is not where this game version expects it");
		return false;
	}

	if (!g_drawHook.InstallRva(GameOffsets::kFnInputHistoryDraw, &HookedDraw))
	{
		LOG("InputHistoryView: the input history drawer is not where this game version expects it");
		return false;
	}

	g_installed = true;
	LOG("InputHistoryView: hooked");
	return true;
}

void InputHistoryView::OnFrame()
{
	if (!g_installed)
		return;

	DrawQueue::Want(DrawQueue::User_InputHistory, Changes() && GameState::IsInMatch());
}

bool InputHistoryView::IsBehind()
{
	return g_modVals.inputHistoryBehind;
}

void InputHistoryView::SetBehind(bool behind)
{
	g_modVals.inputHistoryBehind = behind;
	Settings::SaveInt(kSection, "InputHistoryBehind", behind ? 1 : 0);
}

int InputHistoryView::Rows()
{
	return Bounded(g_modVals.inputHistoryRows);
}

void InputHistoryView::SetRows(int rows)
{
	g_modVals.inputHistoryRows = Bounded(rows);
}

void InputHistoryView::SaveRows()
{
	Settings::SaveInt(kSection, "InputHistoryRows", Rows());
}
