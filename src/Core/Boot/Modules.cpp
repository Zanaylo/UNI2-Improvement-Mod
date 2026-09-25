#include "Core/Boot/Modules.h"

#include "Core/Config/interfaces.h"
#include "Core/Profiler.h"
#include "Core/logger.h"
#include "D3D9/Device/SceneScale.h"
#include "D3D9/Post/SceneUpscale.h"
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
#include "Game/Files/DataSearchPath.h"
#include "Game/Files/ModFiles.h"
#include "Game/Lobby/NameCensor.h"
#include "Game/Lobby/RoomNameCensor.h"
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
#include "Game/Stages/StageObjects.h"
#include "Game/Stages/StagePlacement.h"
#include "Game/Subtitles/SubtitleText.h"
#include "Game/Subtitles/SubtitleWatch.h"
#include "Hooks/InputProbe.h"
#include "Network/ModChannel.h"
#include "Network/NetplayTick.h"
#include "Network/PaletteShare.h"
#include "Overlay/Hud/FrameMeterHud.h"
#include "Overlay/Hud/GrdPopupHud.h"
#include "Overlay/Hud/HealthReadout.h"
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

#include <type_traits>

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
	{ "game hooks: name censor", [] { NameCensor::Install(); } },
	{ "game hooks: room name censor", [] { RoomNameCensor::Install(); } },
	{ "game hooks: random stage", [] { RandomStage::Install(); } },
	{ "game hooks: subtitle watch", [] { SubtitleWatch::Install(); } },
	{ "game hooks: subtitle text", [] { SubtitleText::Install(); } },
	{ "game hooks: stage objects", [] { StageObjects::Initialize(); } },
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
	[] { OstImport::Update(); },
	[] { StageImport::Update(); },
	[] { BgGrade::Update(); },
	[] { BgClear::Update(); },
	[](IDirect3DDevice9* device) { BgMipmaps::Assert(device); },
	[] { StagePlacement::Update(); },
	[] { BattleCockpit::Update(); },
	[] { CharaTint::Update(); },
	[] { StageCards::OnFrame(); },
	[] { VoiceImport::Update(); },
	[] { SoundpackTransfer::Update(); },
	[] { if (UserMusic::ConsumeChanged()) MusicRefresh::Reindex(); },
	[] { CharaSounds::Update(); },
	[] { SubtitleWatch::Update(); },
	[] { BgmControl::OnFrame(); },
	[] { TrainingSave::OnFrame(); },
	[] { TrainingMenuItems::OnFrame(); },
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
	[] { InputProbe::OnFrame(); },
	[] { StageColor::OnFrame(); },
	[] { ExtraStages::OnFrame(); },
	[] { OnlineStage::OnFrame(); },
	[] { PotatoMode::OnFrame(); },
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

void RunTasks(const GroupTable& group, IDirect3DDevice9* device)
{
	for (int i = 0; i < group.count; ++i)
		group.tasks[i].Run(device);
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
	const GroupTable& table = kGroups[group];

	if (table.section == kUntimed)
	{
		RunTasks(table, device);
		return;
	}

	Profiler::Scope scope(table.section);
	RunTasks(table, device);
}
