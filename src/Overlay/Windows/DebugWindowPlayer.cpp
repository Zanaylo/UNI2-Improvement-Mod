#include "Overlay/Windows/DebugWindow.h"

#include "Core/Boot/Compat.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/Camera.h"
#include "Game/Engine/CharaTracker.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/HitboxData.h"
#include "Game/Engine/MemoryMap.h"
#include "Game/Engine/PlayerState.h"
#include "Game/Engine/SaveData.h"
#include "Game/Replays/ReplayState.h"
#include "Game/Tables/GameTables.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Windows/HitboxOverlay.h"
#include "Training/FrameStepper.h"
#include "Training/StopTime.h"

#include <imgui.h>

void DebugWindow::DrawPlayerStateSection()
{
	if (!ImGui::CollapsingHeader("Move state"))
		return;

	if (m_entityCount == 0)
	{
		ImGui::TextDisabled("no entities");
		return;
	}

	if (!ImGui::BeginTable("mvstate", 11, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("#");
	ImGui::TableSetupColumn("pattern");
	ImGui::TableSetupColumn("frame");
	ImGui::TableSetupColumn("act");
	ImGui::TableSetupColumn("atkBox");
	ImGui::TableSetupColumn("stun");
	ImGui::TableSetupColumn("invuln");
	ImGui::TableSetupColumn("shield");
	ImGui::TableSetupColumn("count");
	ImGui::TableSetupColumn("cmd");
	ImGui::TableSetupColumn("moveCode");
	ImGui::TableHeadersRow();

	int inactive = 0;

	for (int i = 0; i < m_entityCount; ++i)
	{
		PlayerState::State state = {};
		if (!PlayerState::Read(m_entities[i], state))
			continue;

		if (!state.hasContext)
		{
			++inactive;
			continue;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%d", i);

		ImGui::TableNextColumn();
		{
			const char* const name = GameTables::PatternName(state.pattern);
			ImGui::Text("%u", state.pattern);

			if (name[0] != '\0' && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", name);
		}
		ImGui::TableNextColumn();
		ImGui::Text("%u", state.frameIndex);

		ImGui::TableNextColumn();
		ImGui::TextColored(state.actionable ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
			: ImVec4(1.0f, 0.5f, 0.3f, 1.0f), state.actionable ? "yes" : "no");

		ImGui::TableNextColumn();
		if (state.attackBoxes < 0)
			ImGui::TextDisabled("-");
		else
			ImGui::TextColored(state.attackBoxes > 0 ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
				: ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%d", state.attackBoxes);

		ImGui::TableNextColumn();
		const uint32_t stun = PlayerState::RemainingStun(state);
		if (stun > 0)
			ImGui::Text("%u (%u+%u)", stun, state.hitstop, state.stunTimer);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		if (PlayerState::IsInvulnerable(state))
			ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%u/%u",
				state.mutekiStrike, state.mutekiThrow);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		if (state.shield != 0 || state.vguardTime != 0)
			ImGui::TextColored(ImVec4(0.7f, 0.4f, 0.95f, 1.0f), "%u/%u", state.shield, state.vguardTime);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		ImGui::Text("%u", state.mvCountFrame);

		ImGui::TableNextColumn();
		if (state.command != 0)
		{
			const char* const name = GameTables::CommandName(state.command);
			ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "%s",
				name[0] != '\0' ? name : "?");
		}
		else
		{
			ImGui::TextDisabled("-");
		}

		ImGui::TableNextColumn();
		if (state.actionKind != 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "%08x", state.actionKind);

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();

				uint32_t mask = 0;
				const char* name = nullptr;

				for (int bit = 0; bit < GameTables::GetMoveCodeBitCount(); ++bit)
				{
					if (GameTables::GetMoveCodeBit(bit, mask, name) &&
						(state.actionKind & mask) != 0)
					{
						ImGui::Text("%s", name);
					}
				}

				ImGui::EndTooltip();
			}
		}
		else
		{
			ImGui::TextDisabled("-");
		}
	}

	ImGui::EndTable();

	if (inactive > 0)
		ImGui::TextDisabled("%d inactive slot(s) hidden", inactive);

	DrawRecorderControls();
}

void DebugWindow::DrawStopTimeSection()
{
	if (!ImGui::CollapsingHeader("Stop time"))
		return;

	if (!StopTime::IsAvailable())
	{
		ImGui::TextDisabled("setter or battle object not resolved");
		return;
	}

	ImGui::InputInt("frames", &m_stopTimeFrames, 1, 30);
	if (m_stopTimeFrames < 0)
		m_stopTimeFrames = 0;

	const bool allowed = GameState::AllowsTrainingTools();
	ImGui::BeginDisabled(!allowed);

	if (ImGui::Button("Apply to all"))
		StopTime::RequestOneShot(m_stopTimeFrames);

	ImGui::SameLine();
	if (ImGui::Button("Clear"))
		StopTime::RequestOneShot(0);

	ImGui::EndDisabled();

	if (!allowed)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(not while online)");
	}

	ImGui::Text("replay: %s%s", ReplayState::GetStatusText(),
		ReplayState::IsPlaying() ? "   -> freezing exactly" : "");

	if (ImGui::TreeNode("Replay candidates"))
	{

		if (ImGui::BeginTable("replaySignals", 3,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("signal");
			ImGui::TableSetupColumn("rva");
			ImGui::TableSetupColumn("value");
			ImGui::TableHeadersRow();

			ReplayState::Signal signal = {};
			for (int i = 0; i < ReplayState::GetSignalCount(); ++i)
			{
				if (!ReplayState::GetSignal(i, signal))
					continue;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(signal.name);

				ImGui::TableNextColumn();
				ImGui::Text("0x%06x", (unsigned)signal.rva);

				ImGui::TableNextColumn();
				if (!signal.read)
					ImGui::TextDisabled("unreadable");
				else if (signal.width == 1)
					ImGui::Text("%u", signal.value);
				else
					ImGui::Text("0x%08x", signal.value);
			}

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}

	ImGui::Text("applies: %llu   rejected: %llu%s%s",
		(unsigned long long)StopTime::GetApplyCount(),
		(unsigned long long)StopTime::GetRejectCount(),
		StopTime::GetLastRejectReason()[0] != '\0' ? "   last reason: " : "",
		StopTime::GetLastRejectReason());

	int count = 0;
	int capacity = 0;
	if (StopTime::ReadQueueState(count, capacity))
		ImGui::Text("message queue: %d of %d", count, capacity);
	else
		ImGui::TextDisabled("message queue not built (enter a match)");

	int queued = 0;
	if (StopTime::ReadQueuedFrames(queued))
		ImGui::Text("queued for all characters: %d", queued);
	else
		ImGui::TextDisabled("nothing queued for all characters (the game empties it every frame)");

	ImGui::Spacing();

	int frames = FrameStepper::GetStopTimeFrames();
	int refresh = FrameStepper::GetStopTimeRefreshTicks();

	bool changed = ImGui::SliderInt("applied frames", &frames, 2, 600);
	changed |= ImGui::SliderInt("renew every N ticks", &refresh, 1, 599);

	if (changed)
		FrameStepper::SetStopTimeTuning(frames, refresh);
}

void DebugWindow::DrawTransformSection()
{
	if (!ImGui::CollapsingHeader("Transform"))
		return;

	ImGui::Text("battle active: %s", CharaTracker::IsBattleActive() ? "yes" : "no");

	float common = 0.0f;
	float scaleX = 0.0f;
	float scaleY = 0.0f;
	if (Camera::GetScales(common, scaleX, scaleY))
		ImGui::Text("scales: common %.8f   x %.8f   y %.8f", common, scaleX, scaleY);
	else
		ImGui::TextDisabled("scales unreadable");

	float matrix[16] = {};
	if (Camera::GetMatrix(matrix))
	{
		for (int row = 0; row < 4; ++row)
		{
			ImGui::Text("m[%d] % 12.4f % 12.4f % 12.4f % 12.4f", row,
				matrix[row * 4 + 0], matrix[row * 4 + 1], matrix[row * 4 + 2], matrix[row * 4 + 3]);
		}
	}
	else
	{
		ImGui::TextDisabled("matrix unreadable");
	}

	const ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("display: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);

	ImGui::Separator();

	for (int i = 0; i < m_entityCount; ++i)
	{
		int worldX = 0;
		int worldY = 0;
		if (!Camera::GetWorldPosition(m_entities[i], worldX, worldY))
			continue;

		const float pixelX = worldX * common;
		const float pixelY = worldY * common;

		float screenX = 0.0f;
		float screenY = 0.0f;
		const bool ok = Camera::PixelToScreen(pixelX, pixelY, screenX, screenY);

		ImGui::Text("[%d] world %7d %7d   pixel %9.2f %9.2f   screen %9.2f %9.2f   facing %+d %s",
			i, worldX, worldY, pixelX, pixelY, screenX, screenY,
			Camera::GetFacing(m_entities[i]), ok ? "" : "(failed)");
	}
}

void DebugWindow::DrawCharaSlotsSection()
{
	if (!ImGui::TreeNode("Chara slots"))
		return;

	ImGui::TextWrapped("One row per engine slot, live. Players and spawned objects share this array.");

	if (ImGui::BeginTable("charaslots", 14, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("#");
		ImGui::TableSetupColumn("address");
		ImGui::TableSetupColumn("on");
		ImGui::TableSetupColumn("objId");
		ImGui::TableSetupColumn("type");
		ImGui::TableSetupColumn("owner");
		ImGui::TableSetupColumn("pat");
		ImGui::TableSetupColumn("frm");
		ImGui::TableSetupColumn("frameObj");
		ImGui::TableSetupColumn("exist");
		ImGui::TableSetupColumn("counts");
		ImGui::TableSetupColumn("world");
		ImGui::TableSetupColumn("face");
		ImGui::TableSetupColumn("boxes");
		ImGui::TableHeadersRow();

		for (int i = 0; i < m_entityCount; ++i)
		{
			void* entity = m_entities[i];

			const bool active = MemoryMap::IsSlotActive(entity);

			uint32_t objectId = 0;
			uint32_t owner = 0;
			uint32_t pattern = 0;
			uint32_t frameIndex = 0;
			MemoryMap::ReadStructDword(entity, GameOffsets::kCharaObjectId, objectId);
			MemoryMap::ReadStructDword(entity, GameOffsets::kCharaOwner, owner);
			MemoryMap::ReadStructDword(entity, GameOffsets::kPlayerDataPattern, pattern);
			MemoryMap::ReadStructDword(entity, GameOffsets::kPlayerDataFrameIndex, frameIndex);

			HitboxData::FrameObject frame = {};
			const bool resolved = HitboxData::Resolve(entity, frame);

			int boxCount = 0;
			if (resolved)
			{
				HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
				boxCount = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);
			}

			int worldX = 0;
			int worldY = 0;
			Camera::GetWorldPosition(entity, worldX, worldY);

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("%d", i);
			ImGui::TableNextColumn();
			ImGui::Text("0x%p", entity);
			ImGui::TableNextColumn();
			if (active)
				ImGui::TextUnformatted("yes");
			else
				ImGui::TextDisabled("-");
			ImGui::TableNextColumn();
			ImGui::Text("0x%x", objectId);
			ImGui::TableNextColumn();
			ImGui::Text("%d", MemoryMap::GetObjectType(entity));
			ImGui::TableNextColumn();
			ImGui::Text("0x%08x", owner);
			ImGui::TableNextColumn();
			ImGui::Text("%u", pattern & 0xffff);
			ImGui::TableNextColumn();
			ImGui::Text("%u", frameIndex & 0xffff);
			ImGui::TableNextColumn();
			if (resolved)
				ImGui::Text("0x%p", frame.pointer);
			else
				ImGui::TextDisabled("failed");
			ImGui::TableNextColumn();
			ImGui::Text("0x%03x", frame.existFlags);
			ImGui::TableNextColumn();
			ImGui::Text("%d/%d/%d/%d", frame.counts[0], frame.counts[1], frame.counts[2],
				frame.counts[3]);
			ImGui::TableNextColumn();
			ImGui::Text("%d,%d", worldX, worldY);
			ImGui::TableNextColumn();
			ImGui::Text("%d", Camera::GetFacing(entity));
			ImGui::TableNextColumn();
			ImGui::Text("%d", boxCount);
		}

		ImGui::EndTable();
	}

	ImGui::TreePop();
}

void DebugWindow::DrawHitboxSection()
{
	if (!ImGui::CollapsingHeader("Hitboxes"))
		return;

	void* entity = GetSelectedEntity();
	if (entity == nullptr)
	{
		ImGui::TextDisabled("no entity selected");
		return;
	}

	ImGui::Text("frame object read from PLAYER_DATA+0x%03x",
		(unsigned)GameOffsets::kCharaFrameObject);

	ImGui::SameLine();
	if (ImGui::Button("Scan for it"))
		m_lastScan = HitboxData::ScanForFrameObject(entity);

	if (m_lastScan.candidates > 0)
	{
		ImGui::TextDisabled("last scan: best +0x%03x with %d box(es), %d candidate offset(s)",
			(unsigned)m_lastScan.offset, m_lastScan.bestScore, m_lastScan.candidates);
	}

	HitboxData::FrameObject frame = {};
	const bool resolved = HitboxData::Resolve(entity, frame);

	if (!resolved)
	{
		ImGui::TextDisabled("frame object not resolved");
		return;
	}

	ImGui::Text("frame object 0x%p at PLAYER_DATA+0x%03x",
		frame.pointer, (unsigned)frame.offsetInPlayerData);
	for (int i = 0; i < HitboxData::kArrayCount; ++i)
	{
		ImGui::Text("  [%d] +0x%03x count %-3d array 0x%p",
			i, (unsigned)(GameOffsets::kFrameObjectArrays + i * 4), frame.counts[i], frame.arrays[i]);
	}

	HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
	const int count = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);

	ImGui::Text("boxes: %d", count);

	if (count == 0)
		return;

	if (ImGui::BeginTable("boxes", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY, Ui::Scaled(0.0f, 220.0f)))
	{
		ImGui::TableSetupColumn("index");
		ImGui::TableSetupColumn("kind");
		ImGui::TableSetupColumn("x1");
		ImGui::TableSetupColumn("y1");
		ImGui::TableSetupColumn("x2");
		ImGui::TableSetupColumn("y2");
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		const int catchBoxIndex = PlayerState::ReadCatchBoxIndex(entity);

		for (int i = 0; i < count; ++i)
		{
			const HitboxData::Box& box = boxes[i];

			const char* kind =
				HitboxOverlay::GetCategoryName(HitboxOverlay::ClassifyBox(box, catchBoxIndex));

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d:%d", box.arrayIndex, box.index);
			ImGui::TableNextColumn();
			ImGui::Text("%s", kind);
			ImGui::TableNextColumn();
			ImGui::Text("%d", box.x1);
			ImGui::TableNextColumn();
			ImGui::Text("%d", box.y1);
			ImGui::TableNextColumn();
			ImGui::Text("%d", box.x2);
			ImGui::TableNextColumn();
			ImGui::Text("%d", box.y2);
		}

		ImGui::EndTable();
	}
}

void DebugWindow::DrawSaveSection()
{
	if (!ImGui::CollapsingHeader("SYS-DATA"))
		return;

	SaveData::State state = {};

	if (!SaveData::Read(state))
	{
		ImGui::TextDisabled("the save globals are not readable yet");
		return;
	}

	const char* const modeName = state.mode == 0 ? "idle"
		: state.mode == 1 ? "load"
		: state.mode == 2 ? "save"
		: "?";

	ImGui::Text("dirty %d   enabled %d   requested %d   buffer %d",
		state.dirty, state.enabled, state.requested, state.buffered);
	ImGui::Text("task mode %d (%s)   machine state %u", state.mode, modeName, state.machine);
	ImGui::Text("header %s   size 0x%x (expects 0x%x)",
		state.headerValid ? "ok" : "bad", state.size, GameOffsets::kSaveFileSize);

	if (!state.enabled)
		ImGui::TextDisabled("saving is switched off, so the dirty flag reads 0");

	if (state.mode == 0)
		ImGui::TextDisabled("the pump is skipped while the mode is idle, so a request may wait");

	if (ImGui::Button("Request save"))
		SaveData::Request();

	ImGui::SameLine();

	if (ImGui::Button("Set dirty flag"))
		SaveData::MarkDirty();

	std::vector<SaveData::File> files;

	if (!SaveData::ListFiles(files))
	{
		ImGui::TextDisabled("no SYS-DATA under the game's Save folder");
		return;
	}

	ImGui::Separator();
	ImGui::Text("%d save folder%s, newest first", static_cast<int>(files.size()),
		files.size() == 1 ? "" : "s");

	for (size_t i = 0; i < files.size(); ++i)
	{
		const SaveData::File& file = files[i];

		FILETIME stamp = {};
		stamp.dwLowDateTime = static_cast<DWORD>(file.written);
		stamp.dwHighDateTime = static_cast<DWORD>(file.written >> 32);

		SYSTEMTIME local = {};
		FileTimeToLocalFileTime(&stamp, &stamp);
		FileTimeToSystemTime(&stamp, &local);

		const size_t start = file.path.find("\\Save\\");
		const char* const shown = start == std::string::npos
			? file.path.c_str() : file.path.c_str() + start + 6;

		if (i == 0)
		{
			ImGui::Text("live  %s  %llu bytes  %04u-%02u-%02u %02u:%02u:%02u", shown, file.size,
				local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
			continue;
		}

		ImGui::TextDisabled("stale %s  %llu bytes  %04u-%02u-%02u %02u:%02u:%02u", shown, file.size,
			local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
	}
}
