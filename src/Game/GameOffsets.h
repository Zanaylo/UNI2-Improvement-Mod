#pragma once

#include <cstddef>
#include <cstdint>

namespace GameOffsets
{
	constexpr uintptr_t kCharaStackBase = 0x5df5f4;
	constexpr uintptr_t kCharaStackTop = 0x5df5f8;

	constexpr uintptr_t kPlayerDataVTable = 0x5456b8;
	constexpr uintptr_t kPlayerDataSize = 0xba4;

	constexpr uintptr_t kEffectVTable = 0x548e68;

	constexpr uintptr_t kEffectArrayBase = 0x85aaf0;
	constexpr uintptr_t kEffectArrayStride = 0x7c0;
	constexpr int kEffectArrayCount = 0x7d0;

	constexpr uintptr_t kCharaArrayBase = 0xc34e80;
	constexpr int kCharaArrayCount = 12;

	constexpr uintptr_t kCharaSlotActive = 0x7bc;
	constexpr uintptr_t kCharaObjectId = 0x8;

	constexpr uintptr_t kCharaExistFlags = 0x84;
	constexpr uint32_t kExistNoKasanariHantei = 0x100;
	constexpr uint32_t kExistNoKuraiHantei = 0x200;
	constexpr uint32_t kExistNoAttackHantei = 0x400;
	constexpr uint32_t kExistNoEtcHantei = 0x800;

	constexpr uintptr_t kCharaObjectType = 0xc;

	constexpr uintptr_t kPlayerDataVectorBegin = 0x8fc;
	constexpr uintptr_t kPlayerDataVectorEnd = 0x900;
	constexpr uintptr_t kPlayerDataVectorCapacity = 0x904;
	constexpr uintptr_t kPlayerDataVectorStride = 0x20;

	constexpr uintptr_t kPlayerDataBaseX = 0x64;
	constexpr uintptr_t kPlayerDataBaseY = 0x68;
	constexpr uintptr_t kPlayerDataOffsetX = 0x70;
	constexpr uintptr_t kPlayerDataOffsetY = 0x74;

	constexpr uintptr_t kPlayerDataFacing = 0x638;

	constexpr uintptr_t kScaleCommon = 0x55bc8c;
	constexpr uintptr_t kScaleX = 0x55bc78;
	constexpr uintptr_t kScaleY = 0x55c0d0;
	constexpr uintptr_t kScreenMatrix = 0x83a938;

	constexpr uintptr_t kPlayerDataPattern = 0x1c;
	constexpr uintptr_t kPlayerDataFrameIndex = 0x20;
	constexpr uintptr_t kPlayerDataFrameFlag = 0x24;
	constexpr uintptr_t kPlayerDataFrameUpdate = 0x30;
	constexpr uintptr_t kPlayerDataMvCountFrame = 0x678;
	constexpr uintptr_t kPlayerDataMvFlagA = 0x670;
	constexpr uintptr_t kPlayerDataMvFlagB = 0x671;
	constexpr uintptr_t kPlayerDataMvValue = 0x674;

	constexpr uintptr_t kPlayerDataActionable = 0x448;

	constexpr uintptr_t kPlayerDataActionLock = 0x1cc;

	constexpr uintptr_t kPlayerDataActionKind = 0x6ac;
	constexpr uint32_t kActionKindTech = 0x01000010;

	constexpr uint32_t kActionKindTechAction = 0x10;

	constexpr uint32_t kActionKindOwnMove = 0x1;

	constexpr uint32_t kActionKindAnyMove = 0x1 | 0x2 | 0x20 | 0x40 | 0x80 | 0x200;

	constexpr uintptr_t kPlayerDataMoveCodeEx = 0x6ac;
	constexpr int kMoveCodeExCount = 8;

	constexpr uint32_t kMoveCode1Jump = 0x400000;

	constexpr uint32_t kMoveCode2FromAssault = 0x10;
	constexpr uint32_t kMoveCode2EnemyAntenStop = 0x100000;

	constexpr uint32_t kMoveCode3Anten = 0x8000;

	constexpr uint32_t kMoveCode6SousaiMuteki = 0x1000;

	constexpr uint32_t kMoveCode7StdAssault = 0x1;
	constexpr uint32_t kMoveCode7AirAssault = 0x2;
	constexpr uint32_t kMoveCode7AssaultLimitAirAtk = 0x800 | 0x1000;

	constexpr uintptr_t kPlayerDataCommand = 0x68c;

	constexpr uint32_t kCommandAssaultAir = 0x18d;
	constexpr uint32_t kCommandDashForward = 0x190;
	constexpr uint32_t kCommandDashBack = 0x191;
	constexpr uint32_t kCommandAssaultGround = 0x40e;

	constexpr uint32_t kCommandForwardShift = 0x40f;
	constexpr uint32_t kCommandJumpFirst = 0x41a;
	constexpr uint32_t kCommandJumpLast = 0x41c;

	constexpr uint32_t kCommandWalkForward = 0x655;
	constexpr uint32_t kCommandWalkBack = 0x656;

	constexpr uintptr_t kPlayerDataRunning = 0x66e;

	constexpr uintptr_t kPlayerDataAirJumpOK = 0x66f;
	constexpr uintptr_t kPlayerDataAirJumpCount = 0x66d;

	constexpr uintptr_t kPlayerDataHitstop = 0x1e4;
	constexpr uintptr_t kPlayerDataStunTimer = 0x228;

	constexpr uintptr_t kPlayerDataInReaction = 0x224;

	constexpr uintptr_t kPlayerDataUkemiTime = 0x218;
	constexpr uint16_t kUkemiNone = 0xffff;

	constexpr uintptr_t kPlayerDataTechWindow = 0x21c;

	constexpr uintptr_t kPlayerDataShield = 0x1ec;
	constexpr uintptr_t kPlayerDataVGuardTime = 0x1ee;

	constexpr uintptr_t kPlayerDataShieldSuccess = 0x1ed;

	constexpr uintptr_t kCharaCounterState = 0x200;

	constexpr uintptr_t kPlayerDataArmorCount = 0x570;
	constexpr uintptr_t kPlayerDataArmorFlag = 0x564;

	constexpr uint32_t kInputButtonMask = 0x0000000fu;
	constexpr int kInputLeverShift = 24;

	constexpr uintptr_t kFnUpdatePlayerInput = 0x1425b0;

	constexpr uintptr_t kRecorderObject = 0x1a648e0;
	constexpr uintptr_t kRecorderLength = 0x10;

	constexpr uintptr_t kRecorderMode = 0x00;
	constexpr uintptr_t kRecorderRunning = 0x04;
	constexpr uintptr_t kRecorderSlot = 0x08;
	constexpr uintptr_t kRecorderCursor = 0x0c;
	constexpr uintptr_t kRecorderCapacity = 0x14;
	constexpr uintptr_t kRecorderStarted = 0x18;
	constexpr uintptr_t kRecorderLiveInput = 0x28;
	constexpr uintptr_t kRecorderOverran = 0x2c;
	constexpr uintptr_t kRecorderTake = 0x30;

	constexpr uint32_t kRecorderModeIdle = 0;
	constexpr uint32_t kRecorderModePlayback = 2;

	constexpr uintptr_t kPlayerDataInputHold = 0x6;
	constexpr uintptr_t kRecorderContainer = 0x1a543f0;
	constexpr uintptr_t kRecorderTakeTable = 0x28;
	constexpr uintptr_t kRecorderTakeStride = 0x60;
	constexpr uintptr_t kRecorderTakeStart = 0x58;

	constexpr uintptr_t kRecorderTakeLength = 0x5c;

	constexpr uintptr_t kRecorderSlotActive = 0x8585c8;

	constexpr uint32_t kRecorderSlotSpan = 0x1999;

	constexpr int kRecorderSlotFrames = 1499;
	constexpr uintptr_t kRecorderDataBase = 0x3e8;

	constexpr uint32_t kRecorderDataSize = 0x10000;

	constexpr int kRecorderSlotCount = 10;

	constexpr uintptr_t kPadRecordBase = 0x83a9d0;
	constexpr uintptr_t kPadRecordStride = 0x1f8;
	constexpr uintptr_t kPadRecordLever = 0x00;
	constexpr uintptr_t kPadRecordButtons = 0x04;

	constexpr uintptr_t kExternalInputFlag = 0x1a34114;
	constexpr uintptr_t kExternalInputLever = 0x1a3408c;
	constexpr uintptr_t kExternalInputButtons = 0x1a34094;

	constexpr uintptr_t kFnGetPlayerInput = 0x13a300;

	constexpr uintptr_t kFnPackInput = 0x139a20;
	constexpr uintptr_t kFnStoreInput = 0x1425b0;
	constexpr uintptr_t kPlayerDataCurrentInput = 0x418;
	constexpr uintptr_t kPlayerDataLeverNumber = 0x41b;
	constexpr uintptr_t kPlayerDataSideIndex = 0x4;

	constexpr uintptr_t kPlayerDataInputDisabled = 0x1f4;

	constexpr uintptr_t kBgPendingNumber = 0x644690;
	constexpr uintptr_t kBgLoadedIndex = 0x595e60;

	constexpr uintptr_t kBgTrainingNumber = 0x858584;

	constexpr uintptr_t kBgStageDrawn = 0x644944;

	constexpr uintptr_t kBgRecordTable = 0x6447b0;
	constexpr uintptr_t kBgRecordViewGrid = 0xec;
	constexpr int kBgRecordCount = 100;

	constexpr int kBgEmptyStage = 99;

	constexpr uintptr_t kBgClearColorImmediate = 0xcdf1a;
	constexpr uint32_t kBgClearColorDefault = 0xff006400;

	constexpr uintptr_t kFrameDisplayPointer = 0x858b84;
	constexpr uintptr_t kFrameDisplayStartup = 0x14;
	constexpr uintptr_t kFrameDisplayTotal = 0x18;

	constexpr uintptr_t kFrameDisplayAdvantage = 0x28;

	constexpr uintptr_t kPlayerDataMutekiA0 = 0x204;
	constexpr uintptr_t kPlayerDataMutekiB0 = 0x205;
	constexpr uintptr_t kPlayerDataMutekiA1 = 0x206;
	constexpr uintptr_t kPlayerDataMutekiB1 = 0x207;

	constexpr uintptr_t kPlayerDataHitCheckInvuln = 0x4a0;
	constexpr uintptr_t kPlayerDataHitCheckInvulnTime = 0x4ac;
	constexpr uintptr_t kPlayerDataHitCheckAttack = 0x4b0;
	constexpr uintptr_t kPlayerDataHitCheckAttackTime = 0x4bc;

	constexpr uint32_t kHitCheckHead = 0x1;
	constexpr uint32_t kHitCheckBody = 0x2;
	constexpr uint32_t kHitCheckLegs = 0x4;
	constexpr uint32_t kHitCheckFireBall = 0x8;
	constexpr uint32_t kHitCheckThrow = 0x10;
	constexpr uint32_t kHitCheckAirDive = 0x40;
	constexpr uint32_t kHitCheckReverse = 0x80;
	constexpr uint32_t kHitCheckLightLegs = 0x100;
	constexpr uint32_t kHitCheckHitToDown = 0x200;

	constexpr uintptr_t kPlayerDataAtemiBox = 0x608;
	constexpr uintptr_t kPlayerDataAtemiTime = 0x614;

	constexpr int kEtcBoxFirst = 9;
	constexpr int kEtcBoxCount = 16;

	constexpr uintptr_t kPlayerDataHurtboxCount = 0x61;

	constexpr uintptr_t kContextStatusWord = 0xfc;
	constexpr uintptr_t kContextStatusA = 0xfe;
	constexpr uintptr_t kContextStatusB = 0xff;
	constexpr uintptr_t kContextStatusC = 0x100;
	constexpr uintptr_t kContextMvStatus = 0x101;

	constexpr uintptr_t kCharaOwner = 0x3f8;

	constexpr uintptr_t kPlayerDataPaletteTable = 0x654;

	constexpr uintptr_t kPlayerDataPaletteTableAlt = 0x754;
	constexpr uintptr_t kPaletteTableFirst = 0x58;
	constexpr uintptr_t kPaletteTableCurrent = 0x4;
	constexpr int kPaletteSlots = 45;

	constexpr uintptr_t kPlayerDataPaletteSlot = 0x7b0;

	constexpr uintptr_t kCharaObjectCount = 0x400;

	constexpr uintptr_t kCharaFrameObject = 0x648;

	constexpr uintptr_t kFrameObjectCounts = 0x114;
	constexpr uintptr_t kFrameObjectArrays = 0x118;

	constexpr uintptr_t kFrameObjectRecord = 0x10c;
	constexpr uintptr_t kCancelFlagsA = 0x0e;
	constexpr uintptr_t kCancelFlagsB = 0x0f;

	constexpr uintptr_t kFrameRecordStance = 0x0c;

	constexpr uintptr_t kFrameRecordInvulnKind = 0x0d;

	constexpr uintptr_t kPlayerDataPosStateValue = 0x470;
	constexpr uintptr_t kPlayerDataPosStateTime = 0x47c;

	constexpr uint8_t kStatusStand = 0;
	constexpr uint8_t kStatusAir = 1;
	constexpr uint8_t kStatusCrouch = 2;

	constexpr uintptr_t kFrameObjectCancelFree = 0x11;
	constexpr uintptr_t kPlayerDataCancelOverride = 0x440;
	constexpr uintptr_t kPlayerDataCancelOverrideStride = 0x10;
	constexpr int kPlayerDataCancelOverrideCount = 2;

	constexpr uint8_t kCancelNever = 0;
	constexpr uint8_t kCancelOnHit = 1;
	constexpr uint8_t kCancelAlways = 2;
	constexpr uint8_t kCancelOnSuccessfulHit = 3;

	constexpr uintptr_t kPlayerDataCancelBoost = 0x460;
	constexpr uintptr_t kPlayerDataCancelBoostTime = 0x46c;
	constexpr uint8_t kCancelBoostNone = 0xff;

	constexpr uintptr_t kFnCheckCancelFree = 0x24b80;
	constexpr uintptr_t kCharaParamBlock = 0x650;
	constexpr uintptr_t kParamArray = 0x9c8;

	constexpr uintptr_t kFnGetPP = 0x48b050;
	constexpr uintptr_t kFnSetPP = 0x48b000;
	constexpr uintptr_t kFnGetPlayerNo = 0x48bce0;
	constexpr uintptr_t kFnGetCharaNo = 0x474580;

	constexpr uintptr_t kCharaRecordBase = 0x8529ac;
	constexpr uintptr_t kCharaRecordStride = 0x2a88;

	constexpr uintptr_t kFnLoadCharaPalette = 0x1c6f10;

	constexpr uintptr_t kFnSetEffectFloatParam = 0x156d0;

	constexpr uintptr_t kFnIsPlayer = 0x4742c0;
	constexpr uintptr_t kFnGetFrameID = 0x48a670;
	constexpr uintptr_t kFnGetPatternNum = 0x47e0a0;
	constexpr uintptr_t kFnGetFrameIDNum = 0x47e070;
	constexpr uintptr_t kFnGetPlayerMuteki = 0x47b8b0;
	constexpr uintptr_t kFnGetPlayerMutekiTimer = 0x47b8e0;
	constexpr uintptr_t kFnGetHanteiRect = 0x47e480;
	constexpr uintptr_t kFnGetScreenPosition = 0x48c6a0;
	constexpr uintptr_t kFnGetPosition = 0x48c760;
	constexpr uintptr_t kFnIsTrainingBattle = 0x489570;
	constexpr uintptr_t kFnFrameProc = 0x489650;
	constexpr uintptr_t kFnPushCharaData = 0x474320;
	constexpr uintptr_t kFnPopCharaData = 0x474310;

	constexpr uintptr_t kComboRecordBase = 0x837408;
	constexpr uintptr_t kComboRecordStride = 0xb0;
	constexpr uintptr_t kComboRecordValid = 0x10;

	constexpr uintptr_t kComboHitCount = 0x28;
	constexpr uintptr_t kComboCandidateB = 0x4c;
	constexpr uintptr_t kComboCandidateC = 0x78;
	constexpr uintptr_t kComboViewValue = 0x2c;
	constexpr uintptr_t kCharaSideIndex = 0x438;

	constexpr uintptr_t kDummyState = 0x1a648e0;
	constexpr uintptr_t kDummyStateB = 0x1a648e4;
	constexpr uintptr_t kDummyStateC = 0x1a648e8;

	constexpr uint32_t kDummyStateIdle = 0;
	constexpr uint32_t kDummyStateCapturing = 1;
	constexpr uint32_t kDummyStatePlaying = 2;

	constexpr uintptr_t kFnDummyPromote = 0x1a9020;

	constexpr uintptr_t kFnDummyActionDriver = 0x1aa790;

	constexpr uintptr_t kPlayerSideIndex = 0x597940;

	constexpr uintptr_t kEnemyStatus = 0x85828c;
	constexpr uint32_t kEnemyStatusController = 4;

	constexpr uintptr_t kFnTrainingHudFlags = 0x1a50a0;

	constexpr uintptr_t kFnInputDisplayRefresh = 0x1a3e80;

	constexpr uintptr_t kTrainingHudRoot = 0x858b70;
	constexpr uintptr_t kHudInputDisplay = 0x10;

	constexpr uintptr_t kInputDisplayStride = 0xe0;
	constexpr uintptr_t kInputDisplayVisible = 0x10;
	constexpr uintptr_t kInputDisplayVisibleP1 = 0x10;
	constexpr uintptr_t kInputDisplayVisibleP2 = 0xf0;

	constexpr uint32_t kInputDisplayOwnSide = 2;
	constexpr uint32_t kInputDisplayOtherSide = 1;

	constexpr uintptr_t kInputDisplayOption = 0x8582dc;

	constexpr uintptr_t kFnFetchPad = 0x1fee00;
	constexpr size_t kPadInputSize = 0x3c;

	constexpr uintptr_t kInputPadSlots = 0x5def24;
	constexpr uintptr_t kInputPadSlotP1 = 0x5def24;
	constexpr uintptr_t kInputPadSlotP2 = 0x5def28;
	constexpr uint32_t kInputPadSlotNone = 0xffffffffu;

	constexpr uintptr_t kFnAssignDummyPad = 0x1a2a00;
	constexpr uintptr_t kFnReleaseDummyPad = 0x1a2bb0;

	constexpr uintptr_t kKeyboardBattleKeys = 0x59a5a0;
	constexpr uintptr_t kKeyboardBattleStride = 14;
	constexpr uintptr_t kKeyboardExtraKeys = 0x5af44a;
	constexpr uintptr_t kKeyboardExtraStride = 2;
	constexpr uintptr_t kKeyboardMenuKeys = 0x59a5bc;
	constexpr uintptr_t kKeyboardMenuStride = 16;
	constexpr uintptr_t kKeyboardSecondPlayer = 0x59a5e4;

	constexpr uintptr_t kFnSampleKeyboard = 0x4d5640;
	constexpr uintptr_t kKeyStateHeld = 0x5e5020;
	constexpr uintptr_t kKeyStateTrigger = 0x5e5140;
	constexpr uintptr_t kKeyStateRepeat = 0x5e5260;
	constexpr uintptr_t kKeyStateReleased = 0x5e56e0;
	constexpr size_t kKeyStateBytesPerPlayer = 0x90;
	constexpr size_t kKeyRepeatBytesPerPlayer = 0x240;

	constexpr uintptr_t kFnPadUpdate = 0x200f80;
	constexpr uintptr_t kReplayArrayPointer = 0x820d18;
	constexpr uintptr_t kReplayStagingArray = 0x1b6c684;
	constexpr uintptr_t kReplayStagingRecord = 0x1b64bf8;
	constexpr uintptr_t kReplayPendingFlag = 0x1b6c680;
	constexpr uintptr_t kReplayTargetRecord = 0x1b6c688;
	constexpr uintptr_t kFnSaveReplay = 0x2279f0;

	constexpr uintptr_t kReplayLoadObject = 0x1a64998;
	constexpr uintptr_t kFnLoadReplayFile = 0x212fe0;
	constexpr uintptr_t kFnStartReplayPlayback = 0x21c170;

	constexpr uintptr_t kFnResetInputLog = 0x20b7e0;
	constexpr uintptr_t kReplayInputLogA = 0x1a351f0;
	constexpr uintptr_t kReplayInputLogB = 0x1a39358;
	constexpr uintptr_t kReplayTakeCountA = 0x1a35200;
	constexpr uintptr_t kReplayTakeCountB = 0x1a39368;
	constexpr uintptr_t kReplayLogFlag = 0x1a351f8;
	constexpr uintptr_t kReplayLogIndex = 0x1a351fc;
	constexpr uintptr_t kFnSetLogName = 0x38e60;
	constexpr uintptr_t kReplayLogFlagB = 0x1a39360;
	constexpr uintptr_t kReplayLogIndexB = 0x1a39364;

	constexpr uintptr_t kFnPlayReplayRecord = 0x40b1c0;
	constexpr uintptr_t kReplayHeaderCopy = 0x3b49d68;

	constexpr uintptr_t kReplayPlaySource = 0x3b49d64;
	constexpr int kReplaySourceList = 0;
	constexpr int kReplaySourceNone = -1;

	constexpr uintptr_t kReplayTakeState = 0x1a64990;
	constexpr uint32_t kReplayTakePlaying = 2;

	constexpr uintptr_t kSceneId = 0x596a84;
	constexpr uintptr_t kSceneRequest = 0x754480;
	constexpr uintptr_t kSceneRequestFlag = 0x596a8c;
	constexpr uintptr_t kSceneResultA = 0x5978d0;
	constexpr uintptr_t kSceneResultB = 0x5978d4;

	constexpr uint32_t kSceneMenu = 3;
	constexpr uint32_t kSceneCharaSelect = 24;
	constexpr uint32_t kSceneReplayList = 46;

	constexpr uintptr_t kReplayListLoaded = 0x3b49d60;
	constexpr uintptr_t kReplayListActive = 0x3b49d61;
	constexpr uintptr_t kReplayListLeaving = 0x3b49d62;
	constexpr uintptr_t kReplayListBuffer = 0x820d20;
	constexpr uintptr_t kReplayListCursor = 0x820d24;
	constexpr uintptr_t kReplayRecordTable = 0x1b8a8fc;

	constexpr uintptr_t kPlayerInfoObjects = 0x1a351f0;
	constexpr size_t kPlayerInfoStride = 0x4168;
	constexpr uintptr_t kPlayerInfoHasId = 0x40e8;
	constexpr uintptr_t kPlayerInfoSteamId = 0x40f0;

	constexpr uintptr_t kInputBlockCount = 0x753dfc;
	constexpr uintptr_t kInputBlockLevel = 0x753e00;
	constexpr uintptr_t kInputBlockStack = 0x753e10;
	constexpr uintptr_t kFnPushInputBlock = 0x1fec30;
	constexpr uintptr_t kFnPopInputBlock = 0x1febc0;
	constexpr uintptr_t kFnFindOldestReplay = 0x225e30;
	constexpr uintptr_t kPadPortState = 0x753f10;
	constexpr size_t kPadPortStateStride = 0x18;

	constexpr uintptr_t kPadPortIsKeyboard = 0x753e08;
	constexpr int kKeyboardPlayerCount = 2;

	constexpr uintptr_t kDummyActionMode = 0x858640;
	constexpr uint32_t kDummyActionModeReversal = 1;

	constexpr uintptr_t kReversalMoves = 0x85858c;
	constexpr uintptr_t kReversalEnabled = 0x8585a0;
	constexpr int kReversalSlotCount = 5;

	constexpr uintptr_t kDummyActionSetting = 0x8582ec;
	constexpr uint32_t kDummyActionReplay = 1;

	constexpr uintptr_t kDummyRecordingStartTiming = 0x8582f0;

	constexpr uintptr_t kBattleObject = 0x83a570;
	constexpr uintptr_t kFnSetStopTime = 0x132830;
	constexpr uintptr_t kFnFrameUpdate = 0x125ea0;

	constexpr uintptr_t kFnBattleLogic = 0x142bd0;
	constexpr uintptr_t kFnEntityUpdate = 0x13fc00;
	constexpr uintptr_t kBattleUpdateReturnSite = 0x141325;

	constexpr uintptr_t kFnPlayerDataDestructor = 0x149310;
	constexpr uintptr_t kFrameCounterA = 0x5978c4;
	constexpr uintptr_t kFrameCounterB = 0x5978c8;

	constexpr uintptr_t kBattleMode = 0x597948;
	constexpr uintptr_t kSubMode = 0x59794c;

	constexpr uintptr_t kVersionString = 0x54eccc;

	constexpr uintptr_t kFnMessageLoop = 0x4d8990;
	constexpr uintptr_t kFnGameThread = 0x4d61a0;
	constexpr uintptr_t kFnRunFrame = 0x4d0790;
	constexpr uintptr_t kFnWindowProc = 0x4d7ac0;
	constexpr uintptr_t kFnPresent = 0xafb70;
	constexpr uintptr_t kFnDeviceReset = 0xb1060;
	constexpr uintptr_t kFnRestoreDevice = 0x4cf950;

	constexpr uintptr_t kFnFrameWait = 0xdbe80;
	constexpr uintptr_t kFrameWaitHasQpc = 0x826758;
	constexpr uintptr_t kFrameWaitFrozenNow = 0x826768;
	constexpr uintptr_t kFrameWaitFrequency = 0x826760;
	constexpr uintptr_t kFrameWaitStart = 0x826778;
	constexpr uintptr_t kFrameWaitSkipOnce = 0x641bdc;
	constexpr uintptr_t kFramePeriodSeconds = 0x55bc98;

	constexpr uintptr_t kRenderPhysicalWidth = 0x5966b4;
	constexpr uintptr_t kRenderPhysicalHeight = 0x5966b8;
	constexpr uintptr_t kRenderVirtualWidth = 0x5966cc;
	constexpr uintptr_t kRenderVirtualHeight = 0x5966d0;
	constexpr uintptr_t kRenderTargetMain = 0x5966a4;
	constexpr uintptr_t kRenderTargetArray = 0x5966a8;
	constexpr uintptr_t kRenderTargetExtra = 0x5966c8;

	constexpr uintptr_t kRenderSizeWidthWrites[] = { 0x4d2153, 0xd3ba7 };
	constexpr uintptr_t kRenderSizeHeightWrites[] = { 0x4d215d, 0xd3bb1 };
	constexpr uintptr_t kRenderSizeWidthLiterals[] = { 0x4d2172, 0x4d219e };
	constexpr uintptr_t kRenderSizeHeightLiterals[] = { 0x4d2182, 0x4d2199 };

	constexpr uintptr_t kRenderVirtualCopyBlock = 0x4d2250;
	constexpr int kRenderVirtualCopyBlockLength = 20;

	constexpr uintptr_t kReferenceHalfWidthLiteral = 0x55c07c;
	constexpr uintptr_t kReferenceHalfHeightLiteral = 0x55c044;

	constexpr uintptr_t kCameraHalfWidth = 0x83a7e4;
	constexpr uintptr_t kCameraHalfHeight = 0x83a7e8;
	constexpr uintptr_t kCameraRcpHalfWidth = 0x83a7ec;
	constexpr uintptr_t kCameraRcpHalfHeight = 0x83a7f0;

	constexpr uintptr_t kReferenceHalfWidth[] = { 0x55bc78, 0x55c0cc };
	constexpr uintptr_t kReferenceHalfHeight[] = { 0x55bc80, 0x55c0d0 };

	constexpr uintptr_t kRenderProjectionWidthOperand = 0x11b674;
	constexpr uintptr_t kRenderProjectionHeightOperand = 0x11b661;
	constexpr uintptr_t kRenderDestWidthOperands[] = { 0x23b6af, 0x23b6f1, 0x23dda6, 0x23dddb };
	constexpr uintptr_t kRenderDestHeightOperands[] = { 0x23b6a7, 0x23b6eb, 0x23dda0, 0x23ddd5 };
	constexpr uintptr_t kRenderBackBufferDestWidthOperands[] = { 0x4d115e, 0x4d2e65 };
	constexpr uintptr_t kRenderBackBufferDestHeightOperands[] = { 0x4d1158, 0x4d2e5f };
	constexpr uintptr_t kRenderBackBufferSourceWidthOperands[] = { 0x4d114e, 0x4d2e55 };
	constexpr uintptr_t kRenderBackBufferSourceHeightOperands[] = { 0x4d1148, 0x4d2e4f };
	constexpr uintptr_t kRenderSwappedDestWidthOperands[] = { 0x43a902, 0x44f705 };
	constexpr uintptr_t kRenderSwappedDestHeightOperands[] = { 0x43a8fc, 0x44f6ff };
	constexpr uintptr_t kRenderSwappedSourceWidthOperands[] = { 0x43a8f2, 0x44f6f5 };
	constexpr uintptr_t kRenderSwappedSourceHeightOperands[] = { 0x43a8ec, 0x44f6ef };
	constexpr uintptr_t kDrawScaleEnabled = 0x5efb0e;
	constexpr uintptr_t kDrawScaleX = 0x641790;
	constexpr uintptr_t kDrawScaleY = 0x64168c;

	constexpr uintptr_t kFnCreateRenderTexture = 0xad8b0;
	constexpr uintptr_t kFnSetMultiSample = 0xad440;

	constexpr uintptr_t kDisplayUseVSync = 0x3b3ae6c;
	constexpr uintptr_t kDisplayResolutionType = 0x3b3ae78;
	constexpr uintptr_t kDisplayFullScreen = 0x3b3ae7c;
	constexpr uintptr_t kDisplayAntialias = 0x3b3ae80;
	constexpr uintptr_t kDisplayCharacterQualityUp = 0x3b3ae8c;

	constexpr uintptr_t kD3dObject = 0x5ee728;
	constexpr uintptr_t kD3dPresentParameters = 0x5c4;
	constexpr uintptr_t kD3dDeviceLost = 0x724;

	constexpr uintptr_t kWindowHasFocus = 0x5e5e66;
	constexpr uintptr_t kWindowHandle = 0x640fa0;

	constexpr uintptr_t kPostFrameSleepMs = 0x595e18;
	constexpr uintptr_t kMessageLoopSleepPush = 0x4d8b1c;
	constexpr uintptr_t kFrameBudgetAccumulator = 0x641bec;

	constexpr uintptr_t kSaveNeededFlag = 0x59a4e4;
	constexpr uintptr_t kSaveRequest = 0x59a4e8;
	constexpr uintptr_t kSaveBuffer = 0x59a4ec;
	constexpr uintptr_t kSaveState = 0x59a500;
	constexpr uintptr_t kSaveTask = 0x59a504;
	constexpr uintptr_t kSaveTaskMode = 0xbc;
	constexpr uintptr_t kSaveEnabled = 0x59a55e;
	constexpr uintptr_t kSaveHeader = 0x59a520;
	constexpr uintptr_t kSaveTotalSize = 0x59a53c;

	constexpr uint32_t kSaveFileSize = 0x7d805;

	constexpr uintptr_t kFnBgmPlay = 0xd6d20;
	constexpr uintptr_t kFnBgmStop = 0xd6b10;
	constexpr uintptr_t kSearchPathBase = 0x3b42eb0;
	constexpr size_t kSearchPathStride = 0x104;
	constexpr int kSearchPathCount = 4;
	constexpr uintptr_t kSearchPathEnabled = 0x59669c;
	constexpr uintptr_t kSearchPathGateWrite[2] = { 0x43be42, 0x43c194 };
	constexpr size_t kSearchPathGateWriteSize = 5;

	constexpr uintptr_t kFnMinGuaranteedDamage = 0x12e040;
	constexpr uintptr_t kCharaHoseiMin = 0x222;
	constexpr uintptr_t kCharaHoseiBaseMin = 0x223;

	constexpr uintptr_t kFnLoadBattleScript = 0x499a30;
	constexpr uintptr_t kBattleScriptContext = 0x3b2d618;
	constexpr uintptr_t kBattleInitPath = 0x553ea4;
	constexpr uintptr_t kBattleInitRoot = 0x553f04;
	constexpr uintptr_t kBattleStdPath = 0x553e7c;
	constexpr uintptr_t kBattleStdRoot = 0x553e94;
	constexpr uintptr_t kComBasePath = 0x5464a0;
	constexpr uintptr_t kComBaseRoot = 0x5464b4;

	constexpr uintptr_t kFnBgmCurrent = 0xd6cd0;
	constexpr uintptr_t kFnBgmStart = 0xd6c20;
	constexpr uintptr_t kFnBgmPause = 0xd6b90;
	constexpr uintptr_t kBgmState = 0x641bd4;
	constexpr uintptr_t kBgmLoadedFlag = 0x641ab8;
	constexpr uintptr_t kFnBgmSelectable = 0x112c10;
	constexpr uintptr_t kBgmCurrentId = 0x595e14;
	constexpr uintptr_t kBgmPlayer = 0x641ac0;
	constexpr uintptr_t kBgmSuppressed = 0x641b4c;
	constexpr int kBgmNetworkMenu = 41;
	constexpr uintptr_t kBgmTableBase = 0x822b60;
	constexpr size_t kBgmTableStride = 0x40;
	constexpr int kBgmSlotCount = 200;
	constexpr uintptr_t kBgmSource = 0x00;
	constexpr uintptr_t kBgmPresent = 0x04;
	constexpr uintptr_t kBgmIsLoop = 0x08;
	constexpr uintptr_t kBgmLoopPos = 0x10;
	constexpr uintptr_t kBgmVolume = 0x18;
	constexpr uintptr_t kBgmNoRecording = 0x1c;
	constexpr uintptr_t kBgmFile = 0x20;
	constexpr int kBgmFileMax = 32;

	constexpr uintptr_t kNetplayActive = 0x5969a0;
	constexpr uintptr_t kNetplayFrame = 0x5969a4;
	constexpr uintptr_t kNetplayFrameB = 0x5969a8;
	constexpr uintptr_t kRollbackCount = 0x822aac;

	constexpr uintptr_t kGgpoSession = 0x3b49858;
	constexpr uintptr_t kGgpoBackendVTable = 0x529900;
	constexpr int kGgpoGetNetworkStats = 8;

	constexpr uintptr_t kFnOnLobbyChatUpdate = 0x101d80;
	constexpr uintptr_t kLobbyChatUpdateUser = 0x08;
	constexpr uintptr_t kLobbyChatUpdateFlags = 0x18;

	constexpr uintptr_t kSessionManager = 0x826820;
	constexpr uintptr_t kSessionManagerLobbyId = 0x826949;

	constexpr uintptr_t kPlayerCardBlock = 0x5af46c;
	constexpr size_t kPlayerCardBlockSize = 0x4000;

	constexpr uintptr_t kCardTitleText = 0x5af4d4;
	constexpr size_t kCardTitleTextBytes = 0x40;

	constexpr uintptr_t kCardPlateFrame = 0x5af5ec;
	constexpr uintptr_t kCardPlatePanel = 0x5af5f0;
	constexpr uintptr_t kCardPlateChara = 0x5af5f4;
	constexpr uintptr_t kCardIp = 0x5af5f8;
	constexpr uintptr_t kCardTitleId = 0x5af5fc;
	constexpr uintptr_t kCardPlateBase = 0x5af600;

	constexpr uintptr_t kTitleWords = 0x59e897;
	constexpr size_t kTitleWordStride = 0x21;
	constexpr int kTitleWordCount = 3;

	constexpr uintptr_t kOwnedFrameList = 0x59ca17;
	constexpr uintptr_t kOwnedPanelList = 0x59ce17;
	constexpr uintptr_t kOwnedCharaList = 0x59d217;
	constexpr uintptr_t kOwnedBaseList = 0x59d617;

	constexpr uintptr_t kOwnedFrameCount = 0x59e8fd;
	constexpr uintptr_t kOwnedPanelCount = 0x59e901;
	constexpr uintptr_t kOwnedCharaCount = 0x59e905;
	constexpr uintptr_t kOwnedBaseCount = 0x59e909;

	constexpr int kOwnedMax = 256;

	constexpr uintptr_t kColourEquipped = 0x5b05f4;
	constexpr uintptr_t kColourUnlockBits = 0x5b09bc;
	constexpr uintptr_t kColourSlots = 0x5b106c;

	constexpr uintptr_t kColourSlotsLive = 0x3b35dc0;
	constexpr uintptr_t kColourUnlockTable = 0x3b49130;

	constexpr int kColourCharacterMax = 64;
	constexpr int kColourUnlockCharacterMax = 32;
	constexpr int kColourUnlockStride = 0x2f;

	constexpr int kColourSlotsPerCharacter = 8;
	constexpr int kColourEntriesPerSlot = 8;

	constexpr int kColourFreeBelow = 10;
	constexpr int kColourUnlockBitCount = 32;
}
