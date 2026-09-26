#include "Overlay/Hud/ProrationHud.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Game/Engine/ComboLedger.h"
#include "Game/Engine/GameDraw.h"
#include "Game/Engine/GameState.h"
#include "Game/Menus/TrainingHud.h"
#include "Game/Menus/TrainingMenu.h"

#include <cstdint>
#include <cstdio>

namespace {

constexpr int kBoxLeft = 449;
constexpr int kBoxTop = 246;
constexpr int kBoxWidth = 384;
constexpr int kBoxHeight = 98;

constexpr int kTitleCentreY = 239;
constexpr int kHeaderCentreY = 256;
constexpr int kFirstRowCentreY = 277;
constexpr int kRowStep = 24;

constexpr int kLabelRight = 84;
constexpr int kCountRight = 160;
constexpr int kRateRight = 246;
constexpr int kDamageRight = 372;

constexpr int kScale = 100;
constexpr int kSmallScale = 75;
constexpr int kBoxLayerOffset = -1;

constexpr uint32_t kFill = 0xC8141420;
constexpr uint32_t kEdge = 0xFF8C8C96;
constexpr uint32_t kTitle = 0xFFB4B4B4;
constexpr uint32_t kHeader = 0xFF8C8C8C;
constexpr uint32_t kLabel = 0xFFC8C8C8;
constexpr uint32_t kValue = 0xFFFFFFFF;
constexpr uint32_t kReduced = 0xFFFFB43C;

void Put(GameDraw::Align align, int x, int centreY, uint32_t colour, const char* text, int layer,
	int scale = kScale)
{
	const GameDraw::Style style = { GameDraw::kCurrentFont, scale };
	const int top = centreY - GameDraw::LineHeight(style) / 2;

	GameDraw::Text(align, kBoxLeft + x, top, text, colour, layer, style);
}

void PutRight(int x, int centreY, uint32_t colour, const char* text, int layer)
{
	Put(GameDraw::Align_Right, x, centreY, colour, text, layer);
}

uint32_t RateColour(int rate)
{
	return rate < ComboProration::kFullRate ? kReduced : kValue;
}

void DrawRow(int row, const char* label, const char* count, int rate, const ComboLedger::Loss& loss,
	int layer)
{
	const int y = kFirstRowCentreY + row * kRowStep;

	char percent[16] = {};
	sprintf_s(percent, "%d%%", rate);

	char damage[32] = {};
	sprintf_s(damage, "%d (-%d)", loss.lastHit, loss.combo);

	PutRight(kLabelRight, y, kLabel, label, layer);
	PutRight(kCountRight, y, kValue, count, layer);
	PutRight(kRateRight, y, RateColour(rate), percent, layer);
	PutRight(kDamageRight, y, kValue, damage, layer);
}

void DrawTotal(const ComboLedger::Totals& totals, int layer)
{
	const int y = kFirstRowCentreY + 2 * kRowStep;

	char damage[32] = {};
	sprintf_s(damage, "%d (-%d)", totals.reading.damage, totals.comboLoss);

	PutRight(kLabelRight, y, kLabel, "TOTAL", layer);
	PutRight(kDamageRight, y, kValue, damage, layer);
}

void DrawPanel(const ComboLedger::Totals& totals, int layer)
{
	Put(GameDraw::Align_Centre, kBoxWidth / 2, kTitleCentreY, kTitle, "Proration info.", layer, kSmallScale);
	Put(GameDraw::Align_Right, kCountRight, kHeaderCentreY, kHeader, "Count", layer, kSmallScale);
	Put(GameDraw::Align_Right, kRateRight, kHeaderCentreY, kHeader, "Proration", layer, kSmallScale);
	Put(GameDraw::Align_Right, kDamageRight, kHeaderCentreY, kHeader, "Damage", layer, kSmallScale);

	char timer[16] = {};
	sprintf_s(timer, "%dF", totals.reading.timer);

	char moves[16] = {};
	sprintf_s(moves, "%d", totals.reading.moves);

	DrawRow(0, "TIMER", timer, totals.reading.timeRate, totals.timer, layer);
	DrawRow(1, "MOVES", moves, totals.reading.moveRate, totals.moves, layer);
	DrawTotal(totals, layer);

	GameDraw::Frame(kBoxLeft, kBoxTop, kBoxWidth, kBoxHeight, kEdge, layer + kBoxLayerOffset);
	GameDraw::Fill(kBoxLeft, kBoxTop, kBoxWidth, kBoxHeight, kFill, layer + kBoxLayerOffset);
}

class Drawer : public TrainingHud::IDrawer
{
public:
	void Draw(int layer) override
	{
		if (!ProrationHud::IsVisible() || !GameState::AllowsTrainingTools() || !GameState::IsTrainingBattle())
			return;

		ComboLedger::Sample();

		if (TrainingMenu::IsActive() || !GameDraw::IsReady())
			return;

		DrawPanel(ComboLedger::Current(), layer);
	}
};

Drawer g_drawer;

}

bool ProrationHud::Install()
{
	return TrainingHud::Install(&g_drawer);
}

bool ProrationHud::IsVisible()
{
	return g_modVals.showProration;
}

void ProrationHud::SetVisible(bool visible)
{
	g_modVals.showProration = visible;
	Settings::SaveInt("Training", "ShowProration", visible ? 1 : 0);
}
