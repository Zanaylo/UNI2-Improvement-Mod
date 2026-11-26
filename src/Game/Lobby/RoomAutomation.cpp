#include "Game/Lobby/RoomAutomation.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/SceneWatch.h"
#include "Hooks/GameHook.h"

#include <Windows.h>

#include <atomic>
#include <cstdio>

namespace {

using RoomAutomation::CreateStage;

typedef void(__stdcall* OpenNetworkMenuFn)(int mode);
typedef void(__thiscall* PlayerMatchResultFn)(void* root, int result);
typedef void(__fastcall* JoinRequestedFn)(void* sessionManager, void* unused, const void* request);

struct JoinRequest
{
	uint64_t lobby;
	uint64_t inviter;
};

constexpr int kSceneRequested = 1;
constexpr unsigned kMainMenuFrames = 30;
constexpr unsigned kMenuSettleFrames = 30;
constexpr unsigned kPickRetryFrames = 60;
constexpr unsigned kCardDismissTimeoutFrames = 90;
constexpr uint32_t kNetworkMenuRunning = 1;
constexpr uint32_t kNetworkListIdle = 1;
constexpr int kNetworkResultCreateRoom = 6;
constexpr int kRematchUnlimited = 10;
constexpr int kMostWinsListed = 10;
constexpr DWORD kGiveUpMs = 120000;

GameHook<OpenNetworkMenuFn> g_menuHook("OpenNetworkMenu");
GameHook<PlayerMatchResultFn> g_resultHook("PlayerMatchResult");

std::atomic<CreateStage> g_stage{ CreateStage::Idle };
std::atomic<int> g_firstTo{ 0 };
DWORD g_stageSince = 0;
unsigned g_menuFrames = 0;
unsigned g_cardDismissFrames = 0;
int g_picks = 0;

int ReadRvaInt(uintptr_t rva)
{
	const uintptr_t address = RvaToAddress(rva);
	uint32_t value = 0;

	if (address == 0 || !TryReadDword(reinterpret_cast<const void*>(address), value))
		return 0;

	return static_cast<int>(value);
}

bool WriteRvaInt(uintptr_t rva, int value)
{
	const uintptr_t address = RvaToAddress(rva);

	return address != 0 && TryWriteDword(reinterpret_cast<void*>(address), static_cast<uint32_t>(value));
}

bool RequestScene(uint32_t scene)
{
	return WriteRvaInt(GameOffsets::kSceneId, static_cast<int>(scene)) &&
		WriteRvaInt(GameOffsets::kSceneRequest, kSceneRequested);
}

uint32_t ReadField(const uint8_t* root, uintptr_t offset)
{
	uint32_t value = 0;
	TryReadDword(root + offset, value);
	return value;
}

uint8_t ReadByte(const uint8_t* root, uintptr_t offset)
{
	uint8_t value = 0;
	TryReadMemory(&value, root + offset, sizeof(value));
	return value;
}

void NoteStage(CreateStage stage)
{
	g_stage.store(stage);
	g_stageSince = GetTickCount();
	g_menuFrames = 0;
	g_cardDismissFrames = 0;
	g_picks = 0;
	LOG("RoomAutomation: %s", RoomAutomation::CreateStageText(stage));
}

int RematchLimitFor(int firstTo)
{
	if (firstTo <= 0 || firstTo > kMostWinsListed)
		return kRematchUnlimited;

	return firstTo - 1;
}

struct SettingRows
{
	uint8_t* first = nullptr;
	int count = 0;
};

SettingRows ReadSettingRows(uint8_t* root, uintptr_t menuOffset)
{
	const uint8_t* const menu = root + menuOffset;
	const uint32_t pagesBegin = ReadField(menu, GameOffsets::kMenuPages);
	const uint32_t pagesEnd = ReadField(menu, GameOffsets::kMenuPages + sizeof(uint32_t));

	SettingRows rows;

	if (pagesBegin == 0 || pagesEnd - pagesBegin < GameOffsets::kMenuPageStride)
		return rows;

	const uint8_t* const firstPage = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(pagesBegin));
	const uint32_t rowsBegin = ReadField(firstPage, 0);
	const uint32_t rowsEnd = ReadField(firstPage, sizeof(uint32_t));

	if (rowsBegin == 0 || rowsEnd < rowsBegin)
		return rows;

	rows.first = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(rowsBegin));
	rows.count = static_cast<int>((rowsEnd - rowsBegin) / GameOffsets::kMenuRowStride);
	return rows;
}

bool SetSetting(const SettingRows& rows, int row, int value)
{
	if (row < 0 || row >= rows.count)
		return false;

	int* const field = reinterpret_cast<int*>(rows.first + row * GameOffsets::kMenuRowStride +
		GameOffsets::kMenuRowValue);
	return TryWriteDword(field, static_cast<uint32_t>(value));
}

void ChooseCreateRoom(uint8_t* root)
{
	const SettingRows rows = ReadSettingRows(root, GameOffsets::kPlayerMatchSettings);
	const uint32_t roomType = ReadField(root, GameOffsets::kPlayerMatchRoomType);
	const bool useAlt = ReadByte(root, GameOffsets::kPlayerMatchUseAltSettings) != 0;
	const int firstTo = g_firstTo.load();
	const int rematches = RematchLimitFor(firstTo);

	if (roomType == 1)
	{
		SetSetting(rows, GameOffsets::kRoomType1RematchRow, rematches);
		SetSetting(rows, GameOffsets::kRoomType1RoundsRow, GameOffsets::kRoomRoundsTwo);
		SetSetting(rows, GameOffsets::kRoomType1RoundTimeRow, GameOffsets::kRoomRoundTimeDefault99);
	}
	else
	{
		SetSetting(rows, GameOffsets::kRoomCapacityRow, GameOffsets::kRoomCapacityTwoPeople);
		SetSetting(rows, GameOffsets::kRoomRematchRow, rematches);
		SetSetting(rows, GameOffsets::kRoomRoundsRow, GameOffsets::kRoomRoundsTwo);
		SetSetting(rows, GameOffsets::kRoomRoundTimeRow, GameOffsets::kRoomRoundTimeDefault99);
	}

	if (useAlt)
	{
		const SettingRows altRows = ReadSettingRows(root, GameOffsets::kPlayerMatchAltSettings);
		SetSetting(altRows, GameOffsets::kRoomAltRematchRow, rematches);
		SetSetting(altRows, GameOffsets::kRoomAltRoundsRow, GameOffsets::kRoomRoundsTwo);
		SetSetting(altRows, GameOffsets::kRoomAltRoundTimeRow, GameOffsets::kRoomRoundTimeDefault99);
	}

	LOG("RoomAutomation: create room type=%u alt=%u rows=%d firstTo=%d rematchOpt=%d rounds=2 time=99",
		roomType, useAlt ? 1u : 0u, rows.count, firstTo, rematches);

	TryWriteDword(root + GameOffsets::kPlayerMatchCreateAnswer,
		static_cast<uint32_t>(GameOffsets::kCreateMenuCreateRoom));
}

bool DismissProfileCardIfVisible()
{
	const uint32_t cardAddr = static_cast<uint32_t>(ReadRvaInt(GameOffsets::kNetworkMenu + GameOffsets::kNetworkMenuProfileCard));

	if (cardAddr == 0)
		return true;

	uint8_t* const card = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(cardAddr));
	uint32_t cardState = 0;

	if (!TryReadDword(card + GameOffsets::kProfileCardState, cardState))
		return true;

	if (cardState == 0 || cardState == GameOffsets::kProfileCardDone)
		return true;

	if (g_cardDismissFrames == 0)
	{
		const uint8_t dismiss = 1;
		TryWriteMemory(card + GameOffsets::kProfileCardDismiss, &dismiss, sizeof(dismiss));
		LOG("RoomAutomation: dismissing network profile card (state=%u)", cardState);
	}

	++g_cardDismissFrames;
	return g_cardDismissFrames >= kCardDismissTimeoutFrames;
}

bool IsCreateScreenReady(uint8_t* root)
{
	if (root == nullptr)
		return false;

	const uint32_t state = ReadField(root, GameOffsets::kPlayerMatchState);
	const uint32_t dialogStep = ReadField(root, GameOffsets::kPlayerMatchDialogStep);
	const uint32_t menuState = ReadField(root, GameOffsets::kPlayerMatchCreateMenu);
	const int answer = static_cast<int>(ReadField(root, GameOffsets::kPlayerMatchCreateAnswer));

	if (state != GameOffsets::kPlayerMatchCreateScreen ||
		dialogStep != GameOffsets::kPlayerMatchDialogOpen ||
		menuState != GameOffsets::kCreateMenuChoosing ||
		answer != GameOffsets::kCreateMenuUnanswered)
	{
		return false;
	}

	return ReadSettingRows(root, GameOffsets::kPlayerMatchSettings).count > 0;
}

bool TryCreateFromCurrentScreen()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kPlayerMatchRoot);

	if (address == 0)
		return false;

	uint8_t* const root = reinterpret_cast<uint8_t*>(address);

	if (!IsCreateScreenReady(root))
		return false;

	NoteStage(CreateStage::CreatingRoom);
	ChooseCreateRoom(root);
	return true;
}

bool IsNetworkMenuRunning()
{
	return static_cast<uint32_t>(ReadRvaInt(GameOffsets::kNetworkMenu + GameOffsets::kNetworkMenuState)) == kNetworkMenuRunning;
}

void StepNetworkMenu()
{
	++g_menuFrames;

	const uint32_t state = static_cast<uint32_t>(ReadRvaInt(GameOffsets::kNetworkMenu + GameOffsets::kNetworkMenuState));
	const uint32_t listState = static_cast<uint32_t>(ReadRvaInt(GameOffsets::kNetworkMenu + GameOffsets::kNetworkMenuList));
	const int pending = ReadRvaInt(GameOffsets::kNetworkMenu + GameOffsets::kNetworkMenuPending);

	if (state != kNetworkMenuRunning)
	{
		if (pending == kNetworkResultCreateRoom)
			NoteStage(CreateStage::OpeningPlayerMatch);
		return;
	}

	if (listState != kNetworkListIdle || g_menuFrames < kMenuSettleFrames)
		return;

	if (!DismissProfileCardIfVisible())
		return;

	if (g_picks > 0 && (g_menuFrames % kPickRetryFrames) != 0)
		return;

	WriteRvaInt(GameOffsets::kNetworkMenu + GameOffsets::kNetworkMenuPick, GameOffsets::kNetworkPickCreateRoom);
	++g_picks;
}

void __stdcall HookedOpenNetworkMenu(int mode)
{
	g_menuHook.Original()(mode);

	const CreateStage stage = g_stage.load();

	if (stage == CreateStage::Armed || stage == CreateStage::EnteringNetwork)
		NoteStage(CreateStage::PickingCreateRoom);
}

void __fastcall HookedPlayerMatchResult(uint8_t* root, void* unused, int result)
{
	g_resultHook.Original()(root, result);

	const CreateStage stage = g_stage.load();

	if (stage != CreateStage::Armed && stage != CreateStage::PickingCreateRoom && stage != CreateStage::OpeningPlayerMatch)
		return;

	if (!IsCreateScreenReady(root))
		return;

	NoteStage(CreateStage::CreatingRoom);
	ChooseCreateRoom(root);
}

bool CallJoinGuarded(JoinRequestedFn handler, void* sessionManager, const JoinRequest* request)
{
	__try
	{
		handler(sessionManager, nullptr, request);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

}

bool RoomAutomation::Install()
{
	if (g_menuHook.IsLive() && g_resultHook.IsLive())
		return true;

	const bool installed = g_menuHook.InstallRva(GameOffsets::kFnOpenNetworkMenu, HookedOpenNetworkMenu) &&
		g_resultHook.InstallRva(GameOffsets::kFnPlayerMatchResult, HookedPlayerMatchResult);

	LOG("RoomAutomation: hooks %s", installed ? "installed" : "not installed");
	return installed;
}

int RoomAutomation::ClaimNetworkPad()
{
	int pad = ReadRvaInt(GameOffsets::kMenuPad);

	if (pad == GameOffsets::kNoPad)
	{
		pad = GameOffsets::kNetworkStartupPad;

		if (!WriteRvaInt(GameOffsets::kMenuPad, pad))
			return GameOffsets::kNoPad;
	}

	return WriteRvaInt(GameOffsets::kNetworkPad, pad) ? pad : GameOffsets::kNoPad;
}

bool RoomAutomation::EnterNetwork()
{
	ClaimNetworkPad();
	return RequestScene(GameOffsets::kSceneNetwork);
}

bool RoomAutomation::LeaveTraining()
{
	return RequestScene(GameOffsets::kSceneMainMenu);
}

bool RoomAutomation::CanJoin()
{
	return CodeSignatures::Address(GameOffsets::kFnOnGameLobbyJoinRequested) != 0 &&
		RvaToAddress(GameOffsets::kSessionManager) != 0;
}

bool RoomAutomation::Join(uint64_t lobby, uint64_t owner)
{
	const uintptr_t handler = CodeSignatures::Address(GameOffsets::kFnOnGameLobbyJoinRequested);
	const uintptr_t sessionManager = RvaToAddress(GameOffsets::kSessionManager);

	if (handler == 0 || sessionManager == 0)
		return false;

	const JoinRequest request = { lobby, owner };
	return CallJoinGuarded(reinterpret_cast<JoinRequestedFn>(handler),
		reinterpret_cast<void*>(sessionManager), &request);
}

void RoomAutomation::StartCreate(int firstTo)
{
	if (!g_menuHook.IsLive() || !g_resultHook.IsLive())
		return;

	g_firstTo.store(firstTo);
	NoteStage(CreateStage::Armed);
}

void RoomAutomation::SetCreateFirstTo(int firstTo)
{
	g_firstTo.store(firstTo);
}

void RoomAutomation::StopCreate()
{
	if (g_stage.load() == CreateStage::Idle)
		return;

	NoteStage(CreateStage::Idle);
}

RoomAutomation::CreateStage RoomAutomation::StepCreate()
{
	const CreateStage stage = g_stage.load();

	if (stage == CreateStage::Idle)
		return stage;

	const uint32_t scene = SceneWatch::Current();

	if (scene == GameOffsets::kSceneNetwork)
	{
		if ((stage == CreateStage::Armed || stage == CreateStage::PickingCreateRoom ||
			stage == CreateStage::OpeningPlayerMatch) && TryCreateFromCurrentScreen())
		{
			return g_stage.load();
		}

		if (stage == CreateStage::Armed && IsNetworkMenuRunning())
		{
			NoteStage(CreateStage::PickingCreateRoom);
			return g_stage.load();
		}
	}

	if (stage == CreateStage::PickingCreateRoom)
	{
		StepNetworkMenu();
		return g_stage.load();
	}

	if (stage == CreateStage::Armed && scene == GameOffsets::kSceneMainMenu &&
		SceneWatch::HeldFrames() >= kMainMenuFrames)
	{
		if (EnterNetwork())
			NoteStage(CreateStage::EnteringNetwork);
	}

	return g_stage.load();
}

RoomAutomation::CreateStage RoomAutomation::CurrentCreateStage()
{
	return g_stage.load();
}

const char* RoomAutomation::CreateStageText(CreateStage stage)
{
	switch (stage)
	{
	case CreateStage::EnteringNetwork:
		return "Opening Network";
	case CreateStage::PickingCreateRoom:
	case CreateStage::OpeningPlayerMatch:
		return "Opening Player Match";
	case CreateStage::CreatingRoom:
		return "Creating the room";
	case CreateStage::Armed:
		return "Go to the main menu and the room is created for you";
	default:
		return "Create a Player Match room and it is sent by itself";
	}
}

bool RoomAutomation::CreateTimedOut()
{
	const CreateStage stage = g_stage.load();

	return stage != CreateStage::Idle && stage != CreateStage::Armed && GetTickCount() - g_stageSince > kGiveUpMs;
}
