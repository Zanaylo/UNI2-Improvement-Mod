#include "Overlay/Window/PlayerControlWindow.h"

#include "Core/utils.h"
#include "Game/Camera.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Palette/PaletteManager.h"
#include "Training/InputLagMeter.h"
#include "Training/PlayerControl.h"

namespace {

constexpr ImU32 kLit = IM_COL32(120, 220, 255, 255);
constexpr ImU32 kDim = IM_COL32(70, 70, 78, 255);
constexpr ImU32 kEdge = IM_COL32(20, 20, 24, 255);

void DrawStick(uint8_t lever, float cell)
{
	const ImVec2 at = ImGui::GetCursorScreenPos();
	ImDrawList* const draw = ImGui::GetWindowDrawList();

	const float pad = 2.0f;

	for (int row = 0; row < 3; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			const int number = 7 - row * 3 + column;
			const bool on = number == static_cast<int>(lever) ||
				(number == 5 && lever == 0);

			const ImVec2 a(at.x + column * cell + pad, at.y + row * cell + pad);
			const ImVec2 b(a.x + cell - pad * 2.0f, a.y + cell - pad * 2.0f);

			draw->AddRectFilled(a, b, on ? kLit : kDim, 2.0f);
			draw->AddRect(a, b, kEdge, 2.0f);
		}
	}

	ImGui::Dummy(ImVec2(cell * 3.0f, cell * 3.0f));
}

void DrawButtons(uint8_t buttons, float size)
{
	const ImVec2 at = ImGui::GetCursorScreenPos();
	ImDrawList* const draw = ImGui::GetWindowDrawList();

	const float radius = size * 0.5f;

	for (int bit = 0; bit < 4; ++bit)
	{
		const bool on = (buttons & (1u << bit)) != 0;
		const ImVec2 centre(at.x + radius + bit * (size + 6.0f), at.y + radius);

		draw->AddCircleFilled(centre, radius, on ? kLit : kDim);
		draw->AddCircle(centre, radius, kEdge);

		const char name[2] = { "ABCD"[bit], '\0' };
		const ImVec2 text = ImGui::CalcTextSize(name);

		draw->AddText(ImVec2(centre.x - text.x * 0.5f, centre.y - text.y * 0.5f),
			on ? IM_COL32(10, 10, 14, 255) : IM_COL32(190, 190, 200, 255), name);
	}

	ImGui::Dummy(ImVec2((size + 6.0f) * 4.0f, size));
}

uint8_t MirrorLever(uint8_t lever, int facing)
{
	if (facing >= 0)
		return lever;

	switch (lever)
	{
	case 1: return 3;
	case 3: return 1;
	case 4: return 6;
	case 6: return 4;
	case 7: return 9;
	case 9: return 7;
	default: return lever;
	}
}

void DrawLagHistoryTable()
{
	const int count = InputLagMeter::GetCount();
	if (count == 0)
		return;

	if (!ImGui::BeginTable("lag_history", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, 240.0f)))
	{
		return;
	}

	ImGui::TableSetupColumn("#");
	ImGui::TableSetupColumn("ms");
	ImGui::TableSetupColumn("from");
	ImGui::TableSetupColumn("quality");
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (int i = 0; i < count; ++i)
	{
		InputLagMeter::Sample sample = {};
		if (!InputLagMeter::GetSample(i, sample))
			break;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%d", i + 1);

		ImGui::TableNextColumn();
		if (sample.trusted)
			ImGui::Text("%.1f", sample.lagMs);
		else
			ImGui::TextDisabled("%.1f", sample.lagMs);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(InputLagMeter::GetSourceName(sample.source));

		ImGui::TableNextColumn();
		if (sample.trusted)
			ImGui::Text("+-%.1f ms", sample.worstGapMs);
		else
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "sampler stalled %.0f ms",
				sample.worstGapMs);
	}

	ImGui::EndTable();
}

}

PlayerControlWindow::PlayerControlWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void PlayerControlWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(480.0f, 640.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 200.0f), ImVec2(FLT_MAX, FLT_MAX));
}

void PlayerControlWindow::Draw()
{
	if (!GameState::AllowsTrainingTools())
	{
		ImGui::TextDisabled("in a match only");
		return;
	}

	const int home = PlayerControl::GetHomeSide();
	const int away = home == 0 ? 1 : 0;
	InputLagMeter::KeepAlive(PlayerControl::GetMode() == PlayerControl::Mode_Other ? away : home);

	DrawButtonCalibration();

	if (ImGui::BeginTable("##sides", 2, ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextRow();

		for (int player = 0; player < 2; ++player)
		{
			ImGui::TableNextColumn();
			DrawSide(player);
		}

		ImGui::EndTable();
	}

	DrawKeyboard();
}

void PlayerControlWindow::DrawSide(int player)
{
	ImGui::PushID(player);

	const int chara = PaletteManager::GetCharaNumber(player);
	const int dummy = PlayerControl::GetDummySide();

	ImGui::Text("P%d  %s", player + 1, PaletteManager::GetCharaName(chara));

	if (player == dummy)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(dummy)");
	}

	PlayerControl::Input input = {};
	PlayerControl::ReadInput(player, input);

	DrawStick(MirrorLever(input.lever, Camera::GetFacing(MemoryMap::GetCharaSlot(player))), 16.0f);
	ImGui::Spacing();
	DrawButtons(input.buttons, 18.0f);

	ImGui::PopID();
}

void PlayerControlWindow::DrawButtonCalibration()
{
	const bool calibrating = PlayerControl::IsCalibrating();

	ImGui::BeginDisabled(calibrating);

	if (ImGui::Button(calibrating ? "Calibrating..." : "Calibrate buttons"))
		PlayerControl::Calibrate();

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Presses each pad button on its own and watches which one lands, so scripts "
			"and Player Control get your character's A/B/C/D right even with a custom button layout. "
			"Takes a few seconds - hold still.");
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s", PlayerControl::GetStatus());
}

void PlayerControlWindow::DrawKeyboard()
{
	ImGui::Spacing();
	ImGui::SeparatorText("My controls drive");

	const int home = PlayerControl::GetHomeSide();
	const int away = home == 0 ? 1 : 0;

	const PlayerControl::Mode forP1 = home == 0 ? PlayerControl::Mode_Mine
		: PlayerControl::Mode_Other;
	const PlayerControl::Mode forP2 = home == 0 ? PlayerControl::Mode_Other
		: PlayerControl::Mode_Mine;

	int mode = static_cast<int>(PlayerControl::GetMode());

	if (ImGui::RadioButton("P1", &mode, forP1))
		PlayerControl::SetMode(forP1);

	ImGui::SameLine();
	if (ImGui::RadioButton("P2", &mode, forP2))
		PlayerControl::SetMode(forP2);

	if (mode == PlayerControl::Mode_Both)
	{
		PlayerControl::SetMode(PlayerControl::Mode_Mine);
		mode = PlayerControl::Mode_Mine;
	}

	switch (mode)
	{
	case PlayerControl::Mode_Mine:
		ImGui::TextDisabled("The game as it comes.");
		break;

	case PlayerControl::Mode_Other:
		ImGui::TextDisabled("Your controls play P%d. P%d stands still.", away + 1, home + 1);
		break;

	default:
		break;
	}

	ImGui::Spacing();

	const float last = InputLagMeter::GetLastMs();
	const float average = InputLagMeter::GetAverageMs();
	const int trusted = InputLagMeter::GetTrustedCount();

	if (last >= 0.0f)
		ImGui::Text("Input lag: %.1f ms", last);
	else
		ImGui::TextDisabled("Input lag: press a key or a button to measure");

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Wall clock from a physical press to this side's own input field "
			"changing. Engine registration lag only, not full click-to-photon.\n\n"
			"The keyboard and every pad are sampled here about a thousand times a second, not read "
			"from the game's own once-a-frame poll - which would quantise every answer to 16.7 ms "
			"and measure nothing.");

	if (average >= 0.0f)
	{
		ImGui::Text("Average %.1f ms over %d (about %.1f frames at 60 Hz)", average, trusted,
			average / (1000.0f / 60.0f));
	}

	const int rate = InputLagMeter::GetSampleRateHz();

	int xinputPads = 0;
	int directInputPads = 0;
	InputLagMeter::GetWatchedDevices(xinputPads, directInputPads);

	if (rate > 0)
	{
		ImGui::TextDisabled("sampler %d Hz, watching the keyboard and %d pad(s)", rate,
			xinputPads + directInputPads);
	}

	ImGui::SameLine();
	if (ImGui::SmallButton("clear history##inputlag"))
		InputLagMeter::Reset();

	DrawLagHistoryTable();
}

void PlayerControlWindow::AfterDraw()
{
	PlayerControl::KeepAlive();
}
