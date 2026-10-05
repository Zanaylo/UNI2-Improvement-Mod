#include "Core/Boot/Modules.h"

#include "Core/Config/interfaces.h"
#include "Core/Harness/CleanFrame.h"
#include "Core/Harness/FrameGrab.h"
#include "Core/Harness/InjectedKeys.h"
#include "Core/Input/BackgroundKeyboard.h"
#include "Core/Profiler.h"
#include "Core/logger.h"
#include "D3D9/Device/SceneScale.h"
#include "D3D9/Post/SceneUpscale.h"
#include "D3D9/Device/StageDetail.h"
#include "Game/Audio/AnnouncerImport.h"
#include "Game/Audio/BgmControl.h"
#include "Game/Audio/CharaSounds.h"
#include "Game/Audio/MusicRefresh.h"
#include "Game/Audio/OstImport.h"
#include "Game/Audio/SoundPacks.h"
#include "Game/Audio/SoundpackTransfer.h"
#include "Game/Audio/UserMusic.h"
#include "Game/Audio/VoiceImport.h"
#include "Game/Battle/BalanceRules.h"
#include "Game/Battle/CharaTint.h"
#include "Game/Battle/GameRestart.h"
#include "Game/Battle/KeyboardSeat.h"
#include "Game/Battle/ScreenShake.h"
#include "Game/Display/PotatoMode.h"
#include "Game/Display/PumpWait.h"
#include "Game/Engine/Camera.h"
#include "Game/Engine/CharaTracker.h"
#include "Game/Engine/HitboxData.h"
#include "Game/Engine/MemoryMap.h"
#include "Game/Engine/SceneWatch.h"
#include "Game/Customize/PortraitCompose.h"
#include "Game/Customize/PortraitDownload.h"
#include "Game/Files/DataSearchPath.h"
#include "Game/Files/ModFiles.h"
#include "Game/Lobby/NameCensor.h"
#include "Game/Lobby/RoomNameCensor.h"
#include "Game/Lobby/RoomStage.h"
#include "Game/Menus/BattleCockpit.h"
#include "Game/Menus/CharaSelectProbe.h"
#include "Game/Patches/GamePatches.h"
#include "Game/Patches/PatchPacks.h"
#include "Game/Replays/ReplayFiles.h"
#include "Game/Replays/ReplayState.h"
#include "Game/Stages/BgClear.h"
#include "Game/Stages/BgGrade.h"
#include "Game/Stages/BgMipmaps.h"
#include "Game/Stages/ExtraStages.h"
#include "Game/Stages/OnlineStage.h"
#include "Game/Stages/RandomStage.h"
#include "Game/Stages/StageCards.h"
#include "Game/Stages/StageImport.h"
#include "Game/Stages/StageCapture.h"
#include "Game/Stages/StageObjects.h"
#include "Game/Stages/StageKick.h"
#include "Game/Stages/StageSampler.h"
#include "Game/Customize/AnnouncerScroll.h"
#include "Game/Stages/StagePlacement.h"
#include "Game/Stages/TextureLoad.h"
#include "Game/Subtitles/SubtitleText.h"
#include "Game/Subtitles/SubtitleWatch.h"
#include "Hooks/InputProbe.h"
#include "Network/ModChannel.h"
#include "Network/NetplayTick.h"
#include "Network/PaletteShare.h"
#include "Overlay/Hud/FrameMeterHud.h"
#include "Overlay/Hud/GrdPopupHud.h"
#include "Overlay/Hud/HealthReadout.h"
#include "Overlay/Hud/ProrationHud.h"
#include "Overlay/Native/DisplaySettingsItems.h"
#include "Overlay/Native/TrainingMenuItems.h"
#include "Palette/EffectOwner.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteBinder.h"
#include "Palette/PaletteChoice.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/PaletteIdentity.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteOwnerProbe.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSeat.h"
#include "Palette/PaletteTexture.h"
#include "Screens/ScreenDirector.h"
#include "Training/Dummy/PlayerControl.h"
#include "Training/FrameStepper.h"
#include "Training/StageColor.h"
#include "Training/TrainingSave.h"
#include "Web/UpdateInstall.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <type_traits>
#include <vector>

namespace {

constexpr DWORD kSlowStageMs = 50;
constexpr Profiler::Section kUntimed = Profiler::Section_COUNT;

using DeviceStep = void (*)(IDirect3DDevice9*);

struct Task
{
	template <typename Plain, std::enable_if_t<std::is_convertible<Plain, Modules::Step>::value, int> = 0>
	constexpr Task(Plain plain)
		: step(plain)
		, deviceStep(nullptr)
	{
	}

	template <typename WithDevice,
		std::enable_if_t<std::is_convertible<WithDevice, DeviceStep>::value, int> = 0>
	constexpr Task(WithDevice withDevice)
		: step(nullptr)
		, deviceStep(withDevice)
	{
	}

	void Run(IDirect3DDevice9* device) const
	{
		if (step != nullptr)
			step();
		else
			deviceStep(device);
	}

	Modules::Step step;
	DeviceStep deviceStep;
};

struct NamedStep
{
	const char* name;
	Modules::Step step;
};

const NamedStep kGameHooks[] = {
	{ "game hooks: chara tracker", [] { CharaTracker::Install(); } },
	{ "game hooks: frame stepper", [] { FrameStepper::Initialize(); } },
	{ "game hooks: player control", [] { PlayerControl::Initialize(); } },
	{ "game hooks: palette memory", [] { PaletteMemory::Install(); } },
	{ "game hooks: palette owner probe", [] { PaletteOwnerProbe::Install(); } },
	{ "game hooks: effect paint", [] { EffectPaint::Install(); } },
	{ "game hooks: palette draw probe", [] { if (g_modVals.showLegacyPalettes) PaletteDrawProbe::Install(); } },
	{ "game hooks: keyboard seat", [] { KeyboardSeat::Initialize(); } },
	{ "game hooks: replay files", [] { ReplayFiles::Initialize(); } },
	{ "game hooks: balance rules", [] { BalanceRules::Install(); } },
	{ "game hooks: screen shake", [] { ScreenShake::Install(); } },
	{ "game hooks: training menu", [] { TrainingMenuItems::Install(); } },
	{ "game hooks: option menu", [] { DisplaySettingsItems::Install(); } },
	{ "game hooks: training hud", [] { ProrationHud::Install(); } },
	{ "game hooks: name censor", [] { NameCensor::Install(); } },
	{ "game hooks: room name censor", [] { RoomNameCensor::Install(); } },
	{ "game hooks: room stage", [] { RoomStage::Install(); } },
	{ "game hooks: random stage", [] { RandomStage::Install(); } },
	{ "game hooks: subtitle watch", [] { SubtitleWatch::Install(); } },
	{ "game hooks: subtitle text", [] { SubtitleText::Install(); } },
	{ "game hooks: stage objects", [] { StageObjects::Initialize(); } },
	{ "game hooks: stage sampler", [] { StageSampler::Initialize(); } },
	{ "game hooks: announcer scroll", [] { AnnouncerScroll::Initialize(); } },
	{ "game hooks: texture load", [] { TextureLoad::Install(); } },
	{ "game hooks: stage cards", [] { StageCards::Initialize(); } },
	{ "game hooks: bgm control", [] { BgmControl::Initialize(); } },
	{ "game hooks: pump wait", [] { PumpWait::Apply(); } },
	{ "game hooks: keyboard seat saved", [] { KeyboardSeat::ApplySaved(); } },
};

const Task kPresentBegin[] = {
	[] { SceneWatch::OnFrame(); },
	[] { if (!ScreenDirector::kOnHold) CharaSelectProbe::OnFrame(); },
	[] { MemoryMap::InvalidateEffectSlotCache(); },
	[] { HitboxData::InvalidateFrameCache(); },
	[](IDirect3DDevice9* device) { StageCapture::OnPresent(device); },
};

const Task kFrame[] = {
	[] { NetplayTick::Update(); },
	[] { GamePatches::Update(); },
	[] { DataSearchPath::Assert(); },
	[] { ModFiles::OnFrame(); },
	[] { PatchPacks::OnFrame(); },
	[] { UpdateInstall::OnFrame(); },
	[] { BalanceRules::OnFrame(); },
	[] { GameRestart::OnFrame(); },
	[] { FrameStepper::Update(); },
	[] { OstImport::Update(); },
	[] { StageImport::Update(); },
	[] { BgGrade::Update(); },
	[] { BgClear::Update(); },
	[](IDirect3DDevice9* device) { BgMipmaps::Assert(device); },
	[] { StagePlacement::Update(); },
	[] { StageKick::Update(); },
	[] { BattleCockpit::Update(); },
	[] { CharaTint::Update(); },
	[] { StageCards::OnFrame(); },
	[] { VoiceImport::Update(); },
	[] { AnnouncerImport::Update(); },
	[] { PortraitCompose::Update(); },
	[] { PortraitDownload::Update(); },
	[] { SoundpackTransfer::Update(); },
	[] { if (UserMusic::ConsumeChanged()) MusicRefresh::Reindex(); },
	[] { CharaSounds::Update(); },
	[] { SubtitleWatch::Update(); },
	[] { BgmControl::OnFrame(); },
	[] { TrainingSave::OnFrame(); },
	[] { TrainingMenuItems::OnFrame(); },
	[] { DisplaySettingsItems::OnFrame(); },
	[] { if (SoundPacks::ConsumeScanRequest()) SoundPacks::Scan(); },
	[] { if (SoundPacks::ConsumeChanged()) ModFiles::Rescan(); },
};

const Task kReplay[] = {
	[] { ReplayState::Update(); },
};

const Task kHud[] = {
	[](IDirect3DDevice9* device) { FrameMeterHud::Render(device); },
	[](IDirect3DDevice9* device) { GrdPopupHud::Render(device); },
	[](IDirect3DDevice9* device) { HealthReadout::Render(device); },
	[](IDirect3DDevice9* device) { TrainingMenuItems::Render(device); },
};

const Task kInput[] = {
	[] { BackgroundKeyboard::OnFrame(); },
	[] { InjectedKeys::OnFrame(); },
	[] { PlayerControl::Update(); },
	[] { KeyboardSeat::Update(); },
	[] { ReplayFiles::Update(); },
};

void LegacyPalettes()
{
	if (!g_modVals.showLegacyPalettes)
		return;

	PaletteDrawProbe::OnFrame();
	PaletteBinder::OnFrame();
	PaletteIdentity::OnFrame();
	PaletteManager::OnFrame();
}

const Task kPalette[] = {
	[] { PaletteControl::OnFrame(); },
	[] { PaletteSeat::OnFrame(); },
	[] { PalettePaint::OnFrame(); },
	[] { EffectOwner::OnFrame(); },
	[] { PaletteChoice::OnFrame(); },
	[] { PaletteTexture::OnFrame(); },
	&LegacyPalettes,
};

const Task kShare[] = {
	[] { ModChannel::Pump(); },
	[] { PaletteShare::OnFrame(); },
};

const Task kTail[] = {
	[] { SceneScale::OnFrame(); },
	[] { Camera::PollDiagnosticRequest(); },
	[] { SceneUpscale::OnPresent(); },
	[] { StageDetail::OnPresent(); },
	[] { InputProbe::OnFrame(); },
	[] { StageColor::OnFrame(); },
	[] { ExtraStages::OnFrame(); },
	[] { OnlineStage::OnFrame(); },
	[] { PotatoMode::OnFrame(); },
	[](IDirect3DDevice9* device) { FrameGrab::OnPresent(device); },
};

struct GroupTable
{
	Profiler::Section section;
	const Task* tasks;
	int count;
};

template <int N>
constexpr GroupTable Table(Profiler::Section section, const Task (&tasks)[N])
{
	return { section, tasks, N };
}

const GroupTable kGroups[Modules::Group_COUNT] = {
	Table(kUntimed, kPresentBegin),
	Table(Profiler::Section_PresentOnline, kFrame),
	Table(Profiler::Section_PresentReplay, kReplay),
	Table(Profiler::Section_PresentMeterHud, kHud),
	Table(kUntimed, kInput),
	Table(Profiler::Section_PresentPalette, kPalette),
	Table(Profiler::Section_PresentShare, kShare),
	Table(kUntimed, kTail),
};

constexpr int kMostTasks = 40;
constexpr const char* kGroupNames[Modules::Group_COUNT] = {
	"begin", "frame", "replay", "hud", "input", "palette", "share", "tail",
};

struct TaskTime
{
	int64_t total;
	int64_t most;
	int calls;
};

TaskTime g_taskTimes[Modules::Group_COUNT][kMostTasks] = {};

void RunTimed(Modules::Group group, IDirect3DDevice9* device)
{
	const GroupTable& table = kGroups[group];

	for (int i = 0; i < table.count && i < kMostTasks; ++i)
	{
		const int64_t began = Profiler::Now();
		table.tasks[i].Run(device);
		const int64_t spent = Profiler::Now() - began;

		TaskTime& time = g_taskTimes[group][i];
		time.total += spent;
		time.most = spent > time.most ? spent : time.most;
		++time.calls;
	}
}

void RunTasks(Modules::Group group, IDirect3DDevice9* device)
{
	if (Profiler::IsEnabled())
	{
		RunTimed(group, device);
		return;
	}

	const GroupTable& table = kGroups[group];

	for (int i = 0; i < table.count; ++i)
		table.tasks[i].Run(device);
}

int LogStageFault(const char* name, DWORD code)
{
	LOG("STAGE '%s' faulted (0x%08lx). Continuing without it - everything after this line was still "
		"installed.", name, static_cast<unsigned long>(code));

	return EXCEPTION_EXECUTE_HANDLER;
}

void RunProtected(const char* name, Modules::Step step)
{
	__try
	{
		step();
	}
	__except (LogStageFault(name, GetExceptionCode()))
	{
	}
}

}

void Modules::RunGuarded(const char* name, Step step)
{
	const DWORD started = GetTickCount();

	RunProtected(name, step);

	const DWORD elapsed = GetTickCount() - started;

	if (elapsed >= kSlowStageMs)
		LOG("stage '%s' took %lu ms", name, static_cast<unsigned long>(elapsed));
}

bool Modules::InstallGameHooks()
{
	static bool ready = false;

	RunGuarded("game hooks: memory map", [] { ready = MemoryMap::Initialize(); });

	if (!ready)
		return false;

	for (const NamedStep& stage : kGameHooks)
		RunGuarded(stage.name, stage.step);

	return true;
}

void Modules::Run(Group group, IDirect3DDevice9* device)
{
	if (group == Group_Hud && CleanFrame::IsOn())
		return;

	if (kGroups[group].section == kUntimed)
	{
		RunTasks(group, device);
		return;
	}

	Profiler::Scope scope(kGroups[group].section);
	RunTasks(group, device);
}

void Modules::ResetTaskTimes()
{
	for (TaskTime (&group)[kMostTasks] : g_taskTimes)
	{
		for (TaskTime& time : group)
			time = {};
	}
}

std::string Modules::SlowestTasks(int count)
{
	std::vector<std::pair<int64_t, std::string> > rows;

	for (int g = 0; g < Group_COUNT; ++g)
	{
		for (int i = 0; i < kMostTasks; ++i)
		{
			const TaskTime& time = g_taskTimes[g][i];

			if (time.calls == 0)
				continue;

			char row[96] = {};
			sprintf_s(row, "%s#%d max %.2fms avg %.3fms", kGroupNames[g], i, Profiler::ToMs(time.most),
				Profiler::ToMs(time.total) / time.calls);
			rows.push_back(std::make_pair(time.most, row));
		}
	}

	std::sort(rows.begin(), rows.end(),
		[](const std::pair<int64_t, std::string>& one, const std::pair<int64_t, std::string>& other)
	{
		return one.first > other.first;
	});

	std::string out;

	for (int i = 0; i < count && i < static_cast<int>(rows.size()); ++i)
		out += "|" + rows[i].second;

	return out;
}
