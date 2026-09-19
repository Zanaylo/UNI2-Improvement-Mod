#pragma once

#include <cstddef>
#include <cstdint>

namespace GameOffsets
{
	constexpr uintptr_t kCharaStackBase = 0x5f2a84;
	constexpr uintptr_t kCharaStackTop = 0x5f2a88;

	constexpr uintptr_t kPlayerDataVTable = 0x5536c8;
	constexpr uintptr_t kPlayerDataSize = 0xbc0;

	constexpr uintptr_t kEffectVTable = 0x556f28;

	constexpr uintptr_t kEffectArrayBase = 0x87d430;
	constexpr uintptr_t kEffectArrayStride = 0x7dc;
	constexpr int kEffectArrayCount = 0x7d0;

	constexpr uintptr_t kCharaArrayBase = 0xc65280;
	constexpr int kCharaArrayCount = 12;

	constexpr uintptr_t kCharaSlotActive = 0x7d8;
	constexpr uintptr_t kCharaObjectId = 0x8;

	constexpr uintptr_t kCharaExistFlags = 0x84;
	constexpr uint32_t kExistNoKasanariHantei = 0x100;
	constexpr uint32_t kExistNoKuraiHantei = 0x200;
	constexpr uint32_t kExistNoAttackHantei = 0x400;
	constexpr uint32_t kExistNoEtcHantei = 0x800;

	constexpr uintptr_t kCharaObjectType = 0xc;

	constexpr uintptr_t kPlayerDataVectorBegin = 0x918;
	constexpr uintptr_t kPlayerDataVectorEnd = 0x91c;
	constexpr uintptr_t kPlayerDataVectorCapacity = 0x920;
	constexpr uintptr_t kPlayerDataVectorStride = 0x20;

	constexpr uintptr_t kPlayerDataBaseX = 0x64;
	constexpr uintptr_t kPlayerDataBaseY = 0x68;
	constexpr uintptr_t kPlayerDataOffsetX = 0x70;
	constexpr uintptr_t kPlayerDataOffsetY = 0x74;

	constexpr uintptr_t kPlayerDataFacing = 0x644;

	constexpr uintptr_t kScaleCommon = 0x569f84;
	constexpr uintptr_t kScaleX = 0x569f70;
	constexpr uintptr_t kScaleY = 0x56a3c8;
	constexpr uintptr_t kScreenMatrix = 0x85d058;

	constexpr uintptr_t kCameraObject = 0x85cca0;
	constexpr uintptr_t kCameraQuakeList = 0x15c;
	constexpr uintptr_t kCameraView = 0x278;
	constexpr uintptr_t kCameraProjection = 0x2f8;
	constexpr uintptr_t kFnCameraQuake = 0x1356d0;

	constexpr uintptr_t kPlayerDataPattern = 0x1c;
	constexpr uintptr_t kPlayerDataFrameIndex = 0x20;
	constexpr uintptr_t kPlayerDataFrameFlag = 0x24;
	constexpr uintptr_t kPlayerDataFrameUpdate = 0x30;
	constexpr uintptr_t kPlayerDataMvCountFrame = 0x684;
	constexpr uintptr_t kPlayerDataMvFlagA = 0x67c;
	constexpr uintptr_t kPlayerDataMvFlagB = 0x67d;
	constexpr uintptr_t kPlayerDataMvValue = 0x680;

	constexpr uintptr_t kPlayerDataActionable = 0x448;

	constexpr uintptr_t kPlayerDataActionLock = 0x1cc;

	constexpr uintptr_t kPlayerDataActionKind = 0x6b8;
	constexpr uint32_t kActionKindTech = 0x01000010;

	constexpr uint32_t kActionKindTechAction = 0x10;

	constexpr uint32_t kActionKindOwnMove = 0x1;

	constexpr uint32_t kActionKindAnyMove = 0x1 | 0x2 | 0x20 | 0x40 | 0x80 | 0x200;

	constexpr uintptr_t kPlayerDataMoveCodeEx = 0x6b8;
	constexpr int kMoveCodeExCount = 8;

	constexpr uint32_t kMoveCode1Jump = 0x400000;

	constexpr uint32_t kMoveCode2FromAssault = 0x10;
	constexpr uint32_t kMoveCode2EnemyAntenStop = 0x100000;

	constexpr uint32_t kMoveCode3Anten = 0x8000;

	constexpr uint32_t kMoveCode6SousaiMuteki = 0x1000;

	constexpr uint32_t kMoveCode7StdAssault = 0x1;
	constexpr uint32_t kMoveCode7AirAssault = 0x2;
	constexpr uint32_t kMoveCode7AssaultLimitAirAtk = 0x800 | 0x1000;

	constexpr uintptr_t kPlayerDataCommand = 0x698;

	constexpr uint32_t kCommandDashForwardDouji = 0x50;
	constexpr uint32_t kCommandDashBackDouji = 0x51;

	constexpr uint32_t kCommandAssaultAir = 0x18d;
	constexpr uint32_t kCommandDashForward = 0x190;
	constexpr uint32_t kCommandDashBack = 0x191;
	constexpr uint32_t kCommandAssaultGround = 0x40e;

	constexpr uint32_t kCommandForwardShift = 0x40f;
	constexpr uint32_t kCommandJumpFirst = 0x41a;
	constexpr uint32_t kCommandJumpLast = 0x41c;

	constexpr uint32_t kCommandWalkForward = 0x655;
	constexpr uint32_t kCommandWalkBack = 0x656;

	constexpr uintptr_t kPlayerDataRunning = 0x67a;

	constexpr uintptr_t kPlayerDataAirJumpOK = 0x67b;
	constexpr uintptr_t kPlayerDataAirJumpCount = 0x679;

	constexpr uintptr_t kPlayerDataHitstop = 0x1de;
	constexpr uintptr_t kPlayerDataStunTimer = 0x228;

	constexpr uintptr_t kPlayerDataInReaction = 0x220;

	constexpr uintptr_t kPlayerDataUkemiTime = 0x218;
	constexpr uint16_t kUkemiNone = 0xffff;

	constexpr uintptr_t kPlayerDataTechWindow = 0x21c;

	constexpr uintptr_t kPlayerDataShield = 0x1e6;
	constexpr uintptr_t kPlayerDataVGuardTime = 0x1e8;

	constexpr uintptr_t kPlayerDataShieldSuccess = 0x1e7;

	constexpr uintptr_t kCharaCounterState = 0x1fc;

	constexpr uintptr_t kPlayerDataArmorCount = 0x570;
	constexpr uintptr_t kPlayerDataArmorFlag = 0x564;

	constexpr uint32_t kInputButtonMask = 0x0000000fu;
	constexpr int kInputLeverShift = 24;

	constexpr uintptr_t kFnUpdatePlayerInput = 0x1431b0;

	constexpr uintptr_t kRecorderObject = 0x1d89db0;
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
	constexpr uintptr_t kRecorderContainer = 0x1d799c0;
	constexpr uintptr_t kRecorderTakeTable = 0x28;
	constexpr uintptr_t kRecorderTakeStride = 0x60;
	constexpr uintptr_t kRecorderTakeStart = 0x58;

	constexpr uintptr_t kRecorderTakeLength = 0x5c;

	constexpr uintptr_t kRecorderSlotActive = 0x87ad10;

	constexpr uint32_t kRecorderSlotSpan = 0x1999;

	constexpr int kRecorderSlotFrames = 1499;
	constexpr uintptr_t kRecorderDataBase = 0x3e8;

	constexpr uint32_t kRecorderDataSize = 0x10000;

	constexpr int kRecorderSlotCount = 10;

	constexpr uintptr_t kPadRecordBase = 0x85d0f0;
	constexpr uintptr_t kPadRecordStride = 0x1f8;
	constexpr uintptr_t kPadRecordLever = 0x00;
	constexpr uintptr_t kPadRecordButtons = 0x04;

	constexpr uintptr_t kExternalInputFlag = 0x1d595ec;
	constexpr uintptr_t kExternalInputLever = 0x1d59564;
	constexpr uintptr_t kExternalInputButtons = 0x1d5956c;

	constexpr uintptr_t kFnGetPlayerInput = 0x13af00;

	constexpr uintptr_t kFnPackInput = 0x13a620;
	constexpr uintptr_t kFnStoreInput = 0x1431b0;
	constexpr uintptr_t kPlayerDataCurrentInput = 0x414;
	constexpr uintptr_t kPlayerDataLeverNumber = 0x41b;
	constexpr uintptr_t kPlayerDataSideIndex = 0x4;

	constexpr uintptr_t kBgPendingNumber = 0x657b80;
	constexpr uintptr_t kBgLoadedIndex = 0x5a3e60;

	constexpr uintptr_t kBgTrainingNumber = 0x87accc;

	constexpr uintptr_t kBgStageDrawn = 0x657e34;

	constexpr uintptr_t kFnLoadStageObjectPat = 0x206260;

	constexpr uintptr_t kFnStageSelectSetup = 0x468fa0;
	constexpr uint32_t kSceneStageSelect = 24;

	constexpr uintptr_t kStageSelectCursor = 0x14;
	constexpr uintptr_t kStageSelectShown = 0x18;
	constexpr uintptr_t kStageSelectSlide = 0x4c;
	constexpr uintptr_t kStageSelectSheet00 = 0xa4;
	constexpr uintptr_t kStageSelectSheet01 = 0xa8;

	constexpr uintptr_t kBgRecordTable = 0x657ca0;
	constexpr uintptr_t kBgSelectList = 0x6579a0;
	constexpr uintptr_t kBgSelectCount = 0x657b84;
	constexpr uintptr_t kBgRecordNameField = 0x20;
	constexpr uintptr_t kBgRecordSelectDisable = 0x60;
	constexpr uintptr_t kBgRecordRandomDisable = 0x64;
	constexpr uintptr_t kBgRecordVsDisable = 0x68;
	constexpr uintptr_t kBgRecordViewGrid = 0xec;
	constexpr int kBgRecordCount = 100;

	constexpr int kBgEmptyStage = 99;

	constexpr uintptr_t kFnStageRandomPick = 0x248150;
	constexpr uintptr_t kFnStageUnlocked = 0x11c020;
	constexpr uintptr_t kStageRandomHeld = 0x6550c0;
	constexpr uintptr_t kStageUnlockSwitch = 0x5f1e48;
	constexpr uintptr_t kStageUnlocks = 0x5bbcd6;
	constexpr uintptr_t kStageUnlocksAlternate = 0x5f1e49;
	constexpr uintptr_t kStageUnlockOffset = 0x40;
	constexpr uintptr_t kRandomIndex = 0x848d60;
	constexpr uintptr_t kRandomSlots = 0x848d64;
	constexpr uintptr_t kRandomDraws = 0x848e44;

	constexpr uintptr_t kBgClearColorImmediate = 0xce12a;
	constexpr uint32_t kBgClearColorDefault = 0xff006400;

	constexpr uintptr_t kFrameDisplayPointer = 0x87b4c4;
	constexpr uintptr_t kFrameDisplayStartup = 0x14;
	constexpr uintptr_t kFrameDisplayTotal = 0x18;

	constexpr uintptr_t kFrameDisplayAdvantage = 0x28;

	constexpr uintptr_t kPlayerDataMutekiA0 = 0x200;
	constexpr uintptr_t kPlayerDataMutekiB0 = 0x201;
	constexpr uintptr_t kPlayerDataMutekiA1 = 0x202;
	constexpr uintptr_t kPlayerDataMutekiB1 = 0x203;

	constexpr uintptr_t kPlayerDataHitCheckInvuln = 0x49c;
	constexpr uintptr_t kPlayerDataHitCheckInvulnTime = 0x4a8;
	constexpr uintptr_t kPlayerDataHitCheckAttack = 0x4ac;
	constexpr uintptr_t kPlayerDataHitCheckAttackTime = 0x4b8;

	constexpr uint32_t kHitCheckHead = 0x1;
	constexpr uint32_t kHitCheckBody = 0x2;
	constexpr uint32_t kHitCheckLegs = 0x4;
	constexpr uint32_t kHitCheckFireBall = 0x8;
	constexpr uint32_t kHitCheckThrow = 0x10;
	constexpr uint32_t kHitCheckAirDive = 0x40;
	constexpr uint32_t kHitCheckReverse = 0x80;
	constexpr uint32_t kHitCheckLightLegs = 0x100;
	constexpr uint32_t kHitCheckHitToDown = 0x200;

	constexpr uintptr_t kPlayerDataAtemiBox = 0x614;
	constexpr uintptr_t kPlayerDataAtemiTime = 0x620;

	constexpr int kEtcBoxFirst = 9;
	constexpr int kEtcBoxCount = 16;

	constexpr uintptr_t kPlayerDataHurtboxCount = 0x61;

	constexpr uintptr_t kContextStatusWord = 0xfc;
	constexpr uintptr_t kContextStatusA = 0xfe;
	constexpr uintptr_t kContextStatusB = 0xff;
	constexpr uintptr_t kContextStatusC = 0x100;
	constexpr uintptr_t kContextMvStatus = 0x101;

	constexpr uintptr_t kCharaOwner = 0x3f4;

	constexpr uintptr_t kPlayerDataPaletteTable = 0x660;

	constexpr uintptr_t kPlayerDataPaletteTableAlt = 0x768;
	constexpr uintptr_t kPaletteTableFirst = 0x58;
	constexpr uintptr_t kPaletteTableCurrent = 0x4;
	constexpr int kPaletteSlots = 45;

	constexpr uintptr_t kPlayerDataPaletteSlot = 0x7b0;

	constexpr uintptr_t kCharaObjectCount = 0x400;

	constexpr uintptr_t kCharaFrameObject = 0x654;

	constexpr uintptr_t kFrameObjectCounts = 0x114;
	constexpr uintptr_t kFrameObjectArrays = 0x118;

	constexpr uintptr_t kFrameObjectRecord = 0x10c;
	constexpr uintptr_t kCancelFlagsA = 0x0e;
	constexpr uintptr_t kCancelFlagsB = 0x0f;

	constexpr uintptr_t kFrameRecordStance = 0x0c;

	constexpr uintptr_t kFrameRecordInvulnKind = 0x0d;

	constexpr uintptr_t kPlayerDataPosStateValue = 0x470;
	constexpr uintptr_t kPlayerDataPosStateTime = 0x478;

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

	constexpr uintptr_t kFnCheckCancelFree = 0x24a20;
	constexpr uintptr_t kCharaParamBlock = 0x65c;
	constexpr uintptr_t kParamArray = 0x9e4;

	constexpr uintptr_t kFnGetPP = 0x498530;
	constexpr uintptr_t kFnSetPP = 0x4984e0;
	constexpr uintptr_t kFnGetPlayerNo = 0x499260;
	constexpr uintptr_t kFnGetCharaNo = 0x481550;

	constexpr uintptr_t kCharaRecordBase = 0x8750ec;
	constexpr uintptr_t kCharaRecordStride = 0x2a88;

	constexpr uintptr_t kFnLoadCharaPalette = 0x1c9ae0;

	constexpr uintptr_t kFnSetEffectFloatParam = 0x15790;

	constexpr uintptr_t kFnIsPlayer = 0x481290;
	constexpr uintptr_t kFnGetFrameID = 0x497b50;
	constexpr uintptr_t kFnGetPatternNum = 0x48b080;
	constexpr uintptr_t kFnGetFrameIDNum = 0x48b050;
	constexpr uintptr_t kFnGetPlayerMuteki = 0x488890;
	constexpr uintptr_t kFnGetPlayerMutekiTimer = 0x4888c0;
	constexpr uintptr_t kFnGetHanteiRect = 0x48b460;
	constexpr uintptr_t kFnGetScreenPosition = 0x499c20;
	constexpr uintptr_t kFnGetPosition = 0x499ce0;
	constexpr uintptr_t kFnIsTrainingBattle = 0x496a50;
	constexpr uintptr_t kFnFrameProc = 0x496b30;
	constexpr uintptr_t kFnPushCharaData = 0x4812f0;
	constexpr uintptr_t kFnPopCharaData = 0x4812e0;

	constexpr uintptr_t kComboRecordBase = 0x859ad8;
	constexpr uintptr_t kComboRecordStride = 0xd8;
	constexpr uintptr_t kComboRecordValid = 0x10;

	constexpr uintptr_t kComboHitCount = 0x28;
	constexpr uintptr_t kComboDamageTotal = 0x4c;
	constexpr uintptr_t kComboCandidateC = 0x78;
	constexpr uintptr_t kComboViewValue = 0x2c;
	constexpr uintptr_t kComboCandidateE = 0x3c;
	constexpr uintptr_t kCharaSideIndex = 0x434;

	constexpr uintptr_t kGrdGaugeBase = 0x874ce0;
	constexpr uintptr_t kGrdGaugeStride = 0x14c;
	constexpr uintptr_t kGrdGaugeBlocks = 0xf0;
	constexpr uintptr_t kGrdGaugePartial = 0xf4;
	constexpr uint32_t kGrdGaugeMaxBlocks = 12;

	constexpr uintptr_t kGrdCycleElapsed = 0x874fb0;
	constexpr uintptr_t kGrdCycleLength = 0x874fb8;
	constexpr int kGrdCycleUnitsPerFrame = 100;

	constexpr uintptr_t kPlayerDataHp = 0x8c;
	constexpr uintptr_t kPlayerDataHpTrailing = 0x90;
	constexpr uintptr_t kPlayerDataHpRecord = 0x868;
	constexpr uintptr_t kHpRecordFirstSegment = 0x10;
	constexpr int kHpRecordSegments = 5;

	constexpr uintptr_t kPlayerDataTintColour = 0x300;
	constexpr uintptr_t kPlayerDataTintTime = 0x304;
	constexpr uintptr_t kPlayerDataTintHold = 0x306;
	constexpr uintptr_t kPlayerDataTintIn = 0x308;
	constexpr uintptr_t kPlayerDataTintType = 0x30a;
	constexpr uint8_t kCharaTintHold = 3;

	constexpr uintptr_t kBattleCockpit = 0x657ee0;
	constexpr uintptr_t kCockpitView = 0x4;
	constexpr uint32_t kCockpitViewShown = 0;
	constexpr uint32_t kCockpitViewHidden = 1;

	constexpr uintptr_t kDummyState = 0x1d89db0;
	constexpr uintptr_t kDummyStateB = 0x1d89db4;
	constexpr uintptr_t kDummyStateC = 0x1d89db8;

	constexpr uint32_t kDummyStateIdle = 0;
	constexpr uint32_t kDummyStateCapturing = 1;
	constexpr uint32_t kDummyStatePlaying = 2;

	constexpr uintptr_t kFnDummyPromote = 0x1abce0;

	constexpr uintptr_t kFnDummyActionDriver = 0x1ad650;

	constexpr uintptr_t kPlayerSideIndex = 0x5a5970;

	constexpr uintptr_t kEnemyStatus = 0x87a9cc;
	constexpr uint32_t kEnemyStatusController = 4;

	constexpr uintptr_t kFnInputDisplayRefresh = 0x1a6380;

	constexpr uintptr_t kTrainingHudRoot = 0x87b4b0;
	constexpr uintptr_t kHudInputDisplay = 0x10;

	constexpr uintptr_t kInputDisplayStride = 0xe0;
	constexpr uintptr_t kInputDisplayVisible = 0x10;
	constexpr uintptr_t kInputDisplayVisibleP1 = 0x10;
	constexpr uintptr_t kInputDisplayVisibleP2 = 0xf0;

	constexpr uint32_t kInputDisplayOwnSide = 2;
	constexpr uint32_t kInputDisplayOtherSide = 1;

	constexpr uintptr_t kInputDisplayOption = 0x87aa1c;

	constexpr uintptr_t kFnFetchPad = 0x203480;
	constexpr size_t kPadInputSize = 0x3c;

	constexpr uintptr_t kInputPadSlots = 0x5f23a4;
	constexpr uintptr_t kInputPadSlotP1 = 0x5f23a4;
	constexpr uintptr_t kInputPadSlotP2 = 0x5f23a8;
	constexpr uint32_t kInputPadSlotNone = 0xffffffffu;

	constexpr uintptr_t kFnAssignDummyPad = 0x1a4ee0;
	constexpr uintptr_t kFnReleaseDummyPad = 0x1a5090;

	constexpr uintptr_t kKeyboardBattleKeys = 0x5a8670;
	constexpr uintptr_t kKeyboardBattleStride = 14;
	constexpr uintptr_t kKeyboardExtraKeys = 0x5bd51a;
	constexpr uintptr_t kKeyboardExtraStride = 2;
	constexpr uintptr_t kKeyboardMenuKeys = 0x5a868c;
	constexpr uintptr_t kKeyboardMenuStride = 16;
	constexpr uintptr_t kKeyboardSecondPlayer = 0x5a86b4;

	constexpr uintptr_t kFnSampleKeyboard = 0x4e28f0;
	constexpr uintptr_t kKeyStateHeld = 0x5f87f0;
	constexpr uintptr_t kKeyStateTrigger = 0x5f8910;
	constexpr uintptr_t kKeyStateRepeat = 0x5f8a30;
	constexpr uintptr_t kKeyStateReleased = 0x5f8eb0;
	constexpr size_t kKeyStateBytesPerPlayer = 0x90;
	constexpr size_t kKeyRepeatBytesPerPlayer = 0x240;

	constexpr uintptr_t kFnPadUpdate = 0x2054c0;
	constexpr uintptr_t kReplayArrayPointer = 0x843404;
	constexpr uintptr_t kReplayStagingArray = 0x1e91c04;
	constexpr uintptr_t kReplayStagingRecord = 0x1e8a178;
	constexpr uintptr_t kReplayPendingFlag = 0x1e91c00;
	constexpr uintptr_t kReplayTargetRecord = 0x1e91c08;
	constexpr uintptr_t kFnSaveReplay = 0x22d540;

	constexpr uintptr_t kReplayLoadObject = 0x1d89f18;
	constexpr uintptr_t kFnLoadReplayFile = 0x218190;
	constexpr uintptr_t kFnStartReplayPlayback = 0x221250;

	constexpr uintptr_t kFnResetInputLog = 0x210ab0;
	constexpr uintptr_t kReplayInputLogA = 0x1d5a550;
	constexpr uintptr_t kReplayInputLogB = 0x1d5e6f0;
	constexpr uintptr_t kReplayTakeCountA = 0x1d5a560;
	constexpr uintptr_t kReplayTakeCountB = 0x1d5e700;
	constexpr uintptr_t kReplayLogFlag = 0x1d5a558;
	constexpr uintptr_t kReplayLogIndex = 0x1d5a55c;
	constexpr uintptr_t kFnSetLogName = 0x38db0;
	constexpr uintptr_t kReplayLogFlagB = 0x1d5e6f8;
	constexpr uintptr_t kReplayLogIndexB = 0x1d5e6fc;

	constexpr uintptr_t kFnPlayReplayRecord = 0x415830;
	constexpr uintptr_t kReplayHeaderCopy = 0x4448d60;

	constexpr uintptr_t kReplayPlaySource = 0x4448fd8;
	constexpr int kReplaySourceList = 0;
	constexpr int kReplaySourceNone = -1;

	constexpr uintptr_t kReplayTakeState = 0x1d89f10;
	constexpr uint32_t kReplayTakePlaying = 2;

	constexpr uintptr_t kSceneId = 0x5a4764;
	constexpr uintptr_t kSceneRequest = 0x776b64;
	constexpr uintptr_t kSceneRequestFlag = 0x5a476c;
	constexpr uintptr_t kSceneResultA = 0x5a5900;
	constexpr uintptr_t kSceneResultB = 0x5a5904;

	constexpr uint32_t kSceneMenu = 3;
	constexpr uint32_t kSceneCharaSelect = 24;
	constexpr uint32_t kSceneReplayList = 46;

	constexpr uintptr_t kReplayListLoaded = 0x44488c5;
	constexpr uintptr_t kReplayListActive = 0x44488c6;
	constexpr uintptr_t kReplayListLeaving = 0x44488c7;
	constexpr uintptr_t kReplayListBuffer = 0x84340c;
	constexpr uintptr_t kReplayListCursor = 0x843410;
	constexpr uintptr_t kReplayRecordTable = 0x1eafecc;

	constexpr uintptr_t kFnResolvePersonaName = 0x2fa090;
	constexpr uintptr_t kFnSetFallbackName = 0x38db0;

	constexpr uintptr_t kFnSetRoomRows = 0x2ea8b0;
	constexpr size_t kRoomRowStride = 0x220;
	constexpr size_t kRoomRowName = 0x8;
	constexpr size_t kRoomRowNameSize = 0x10;
	constexpr size_t kRoomRowNameCapacity = 0x14;

	constexpr uintptr_t kPlayerInfoObjects = 0x1d5a550;
	constexpr size_t kPlayerInfoStride = 0x4168;
	constexpr uintptr_t kPlayerInfoHasId = 0x40e8;
	constexpr uintptr_t kPlayerInfoSteamId = 0x40f0;

	constexpr uintptr_t kInputBlockCount = 0x7762f0;
	constexpr uintptr_t kInputBlockLevel = 0x7762f4;
	constexpr uintptr_t kInputBlockStack = 0x776300;
	constexpr uintptr_t kFnPushInputBlock = 0x2032b0;
	constexpr uintptr_t kFnPopInputBlock = 0x203240;
	constexpr uintptr_t kFnFindOldestReplay = 0x22b980;
	constexpr uintptr_t kPadPortState = 0x776410;
	constexpr size_t kPadPortStateStride = 0x18;

	constexpr uintptr_t kPadPortIsKeyboard = 0x776400;
	constexpr int kKeyboardPlayerCount = 2;

	constexpr uintptr_t kDummyActionMode = 0x87ad88;
	constexpr uint32_t kDummyActionModeReversal = 1;

	constexpr uintptr_t kReversalMoves = 0x87acd4;
	constexpr uintptr_t kReversalEnabled = 0x87ace8;
	constexpr int kReversalSlotCount = 5;

	constexpr uintptr_t kDummyActionSetting = 0x87aa2c;
	constexpr uint32_t kDummyActionReplay = 1;

	constexpr uintptr_t kDummyRecordingStartTiming = 0x87aa30;

	constexpr uintptr_t kBattleObject = 0x85cc90;
	constexpr uintptr_t kFnSetStopTime = 0x133380;
	constexpr uintptr_t kFnFrameUpdate = 0x126660;

	constexpr uintptr_t kFnBattleLogic = 0x1437d0;
	constexpr uintptr_t kFnEntityUpdate = 0x140800;
	constexpr uintptr_t kBattleUpdateReturnSite = 0x141f25;

	constexpr uintptr_t kFnPlayerDataDestructor = 0x14a080;
	constexpr uintptr_t kFrameCounterA = 0x5a58f4;
	constexpr uintptr_t kFrameCounterB = 0x5a58f8;

	constexpr uintptr_t kBattleMode = 0x5a5978;
	constexpr uintptr_t kSubMode = 0x5a597c;

	constexpr uintptr_t kVersionString = 0x55cdb4;

	constexpr uintptr_t kFnMessageLoop = 0x4e5c30;
	constexpr uintptr_t kFnGameThread = 0x4e3450;
	constexpr uintptr_t kFnRunFrame = 0x4dda40;
	constexpr uintptr_t kFnWindowProc = 0x4e4d60;
	constexpr uintptr_t kFnPresent = 0xafd10;
	constexpr uintptr_t kFnDeviceReset = 0xb1200;
	constexpr uintptr_t kFnRestoreDevice = 0x4dcbf0;

	constexpr uintptr_t kFnFrameWait = 0xdc0b0;
	constexpr uintptr_t kFrameWaitHasQpc = 0x848e48;
	constexpr uintptr_t kFrameWaitFrozenNow = 0x848e58;
	constexpr uintptr_t kFrameWaitFrequency = 0x848e50;
	constexpr uintptr_t kFrameWaitStart = 0x848e68;
	constexpr uintptr_t kFrameWaitSkipOnce = 0x6550cc;
	constexpr uintptr_t kFramePeriodSeconds = 0x569f90;

	constexpr uintptr_t kRenderPhysicalWidth = 0x5a46b4;
	constexpr uintptr_t kRenderPhysicalHeight = 0x5a46b8;
	constexpr uintptr_t kRenderVirtualWidth = 0x5a46cc;
	constexpr uintptr_t kRenderVirtualHeight = 0x5a46d0;
	constexpr uintptr_t kRenderTargetMain = 0x5a46a4;
	constexpr uintptr_t kRenderTargetArray = 0x5a46a8;
	constexpr uintptr_t kRenderTargetExtra = 0x5a46c8;

	constexpr uintptr_t kRenderSizeWidthWrites[] = { 0x4df403, 0xd3dc7 };
	constexpr uintptr_t kRenderSizeHeightWrites[] = { 0x4df40d, 0xd3dd1 };
	constexpr uintptr_t kRenderSizeWidthLiterals[] = { 0x4df422, 0x4df44e };
	constexpr uintptr_t kRenderSizeHeightLiterals[] = { 0x4df432, 0x4df449 };

	constexpr uintptr_t kStageTargetWidthStores[] = { 0x11bc2e, 0x11bcdc, 0x11bd20, 0x11bd68 };
	constexpr uintptr_t kStageTargetWidthArgs[] = { 0x11bc62, 0x11bc78, 0x11bc92, 0x11bcad };
	constexpr uintptr_t kStageViewportWidthStore = 0x11bd97;

	constexpr uintptr_t kRenderVirtualCopyBlock = 0x4df500;
	constexpr int kRenderVirtualCopyBlockLength = 20;

	constexpr uintptr_t kReferenceHalfWidthLiteral = 0x56a374;
	constexpr uintptr_t kReferenceHalfHeightLiteral = 0x56a33c;

	constexpr uintptr_t kCameraHalfWidth = 0x85cf04;
	constexpr uintptr_t kCameraHalfHeight = 0x85cf08;
	constexpr uintptr_t kCameraRcpHalfWidth = 0x85cf0c;
	constexpr uintptr_t kCameraRcpHalfHeight = 0x85cf10;

	constexpr uintptr_t kReferenceHalfWidth[] = { 0x569f70, 0x56a3c4 };
	constexpr uintptr_t kReferenceHalfHeight[] = { 0x569f78, 0x56a3c8 };

	constexpr uintptr_t kRenderProjectionWidthOperand = 0x11bab4;
	constexpr uintptr_t kRenderProjectionHeightOperand = 0x11baa1;
	constexpr uintptr_t kRenderDestWidthOperands[] = { 0x2415bf, 0x241601, 0x243c94, 0x243cc9 };
	constexpr uintptr_t kRenderDestHeightOperands[] = { 0x2415b7, 0x2415fb, 0x243c8e, 0x243cc3 };
	constexpr uintptr_t kRenderBackBufferDestWidthOperands[] = { 0x4de40e, 0x4e0115 };
	constexpr uintptr_t kRenderBackBufferDestHeightOperands[] = { 0x4de408, 0x4e010f };
	constexpr uintptr_t kRenderBackBufferSourceWidthOperands[] = { 0x4de3fe, 0x4e0105 };
	constexpr uintptr_t kRenderBackBufferSourceHeightOperands[] = { 0x4de3f8, 0x4e00ff };
	constexpr uintptr_t kRenderSwappedDestWidthOperands[] = { 0x445f32, 0x45c265 };
	constexpr uintptr_t kRenderSwappedDestHeightOperands[] = { 0x445f2c, 0x45c25f };
	constexpr uintptr_t kRenderSwappedSourceWidthOperands[] = { 0x445f22, 0x45c255 };
	constexpr uintptr_t kRenderSwappedSourceHeightOperands[] = { 0x445f1c, 0x45c24f };
	constexpr uintptr_t kDrawScaleEnabled = 0x602ffe;
	constexpr uintptr_t kDrawScaleX = 0x654c80;
	constexpr uintptr_t kDrawScaleY = 0x654c7c;

	constexpr uintptr_t kFnCreateRenderTexture = 0xada50;
	constexpr uintptr_t kFnSetMultiSample = 0xad5e0;

	constexpr uintptr_t kDisplayUseVSync = 0x443907c;
	constexpr uintptr_t kDisplayResolutionType = 0x4439088;
	constexpr uintptr_t kDisplayFullScreen = 0x443908c;
	constexpr uintptr_t kDisplayAntialias = 0x4439090;
	constexpr uintptr_t kDisplayCharacterQualityUp = 0x443909c;

	constexpr uintptr_t kD3dObject = 0x601c18;
	constexpr uintptr_t kD3dPresentParameters = 0x5c4;
	constexpr uintptr_t kD3dDeviceLost = 0x724;

	constexpr uintptr_t kWindowHasFocus = 0x5f9306;
	constexpr uintptr_t kWindowHandle = 0x654490;

	constexpr uintptr_t kPostFrameSleepMs = 0x5a3e18;
	constexpr uintptr_t kMessageLoopSleepPush = 0x4e5dbc;
	constexpr uintptr_t kFrameBudgetAccumulator = 0x6550dc;

	constexpr uintptr_t kSaveNeededFlag = 0x5a85b4;
	constexpr uintptr_t kSaveRequest = 0x5a85b8;
	constexpr uintptr_t kSaveBuffer = 0x5a85bc;
	constexpr uintptr_t kSaveState = 0x5a85d0;
	constexpr uintptr_t kSaveTask = 0x5a85d4;
	constexpr uintptr_t kSaveTaskMode = 0xbc;
	constexpr uintptr_t kSaveEnabled = 0x5a862e;
	constexpr uintptr_t kSaveHeader = 0x5a85f0;
	constexpr uintptr_t kSaveTotalSize = 0x5a860c;

	constexpr uint32_t kSaveFileSize = 0x7d805;

	constexpr uintptr_t kFnBgmPlay = 0xd6f50;
	constexpr uintptr_t kFnBgmStop = 0xd6d40;
	constexpr uintptr_t kSearchPathBase = 0x44410c0;
	constexpr size_t kSearchPathStride = 0x104;
	constexpr int kSearchPathCount = 4;
	constexpr uintptr_t kSearchPathEnabled = 0x5a4698;
	constexpr uintptr_t kSearchPathGateWrite[2] = { 0x447472, 0x4477c4 };
	constexpr size_t kSearchPathGateWriteSize = 5;

	constexpr uintptr_t kFnMinGuaranteedDamage = 0x12e9a0;
	constexpr uintptr_t kCharaHoseiMin = 0x21e;
	constexpr uintptr_t kCharaHoseiBaseMin = 0x21f;

	constexpr uintptr_t kFnLoadBattleScript = 0x4a6c70;
	constexpr uintptr_t kBattleScriptContext = 0x4401008;
	constexpr uintptr_t kBattleInitPath = 0x562090;
	constexpr uintptr_t kBattleInitRoot = 0x5620c4;
	constexpr uintptr_t kBattleStdPath = 0x5620ac;
	constexpr uintptr_t kBattleStdRoot = 0x5620d0;
	constexpr uintptr_t kComBasePath = 0x55458c;
	constexpr uintptr_t kComBaseRoot = 0x5545a0;

	constexpr uintptr_t kFnReloadBattleScripts = 0x4dca70;
	constexpr uintptr_t kFnLoadVectorTable = 0x129260;
	constexpr uintptr_t kFnLoadBattleInfo = 0x12fd90;
	constexpr uintptr_t kBattleVmCreated = 0x4401004;
	constexpr uintptr_t kVectorTable = 0x657e70;
	constexpr uintptr_t kRunningResourceThread = 0x843cc8;
	constexpr size_t kVectorSetAt = 0x4;
	constexpr size_t kVectorRecordsBeginAt = 0x10;
	constexpr size_t kVectorRecordsEndAt = 0x14;
	constexpr size_t kVectorRecordStride = 0x94;

	constexpr uintptr_t kFnBgmCurrent = 0xd6f00;
	constexpr uintptr_t kFnBgmStart = 0xd6e50;
	constexpr uintptr_t kFnBgmPause = 0xd6dc0;
	constexpr uintptr_t kBgmState = 0x6550c4;
	constexpr uintptr_t kBgmLoadedFlag = 0x654fa8;
	constexpr uintptr_t kFnBgmSelectable = 0x112f00;
	constexpr uintptr_t kBgmCurrentId = 0x5a3e14;
	constexpr uintptr_t kBgmPlayer = 0x654fb0;
	constexpr uintptr_t kBgmSuppressed = 0x65503c;
	constexpr int kBgmNetworkMenu = 41;
	constexpr uintptr_t kBgmTableBase = 0x845250;
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

	constexpr uintptr_t kBgmTrackVolume = 0x5a3e08;
	constexpr uintptr_t kBgmBaseVolume = 0x5a3e0c;
	constexpr uintptr_t kFnBgmSetVolume = 0xd6830;
	constexpr uintptr_t kFnStreamSetVolume = 0xb3110;

	constexpr uintptr_t kFnMenuBgmChoose = 0x2d4e20;
	constexpr uintptr_t kFnBgmSeek = 0xb2420;

	constexpr uintptr_t kFnSePlay = 0xd9d70;
	constexpr uintptr_t kFnBattlePlayerSeLoad = 0x1239f0;
	constexpr uintptr_t kFnExStringText = 0x42c7b0;
	constexpr uintptr_t kFnCharaSetMessage = 0x485a70;
	constexpr int kBattlePlayerSeSlots = 16;
	constexpr uintptr_t kMenuBgmRemembered = 0x5a7f88;

	constexpr uintptr_t kNetplayActive = 0x5a49b0;
	constexpr uintptr_t kNetplayFrame = 0x5a49b4;
	constexpr uintptr_t kNetplayFrameB = 0x5a49b8;
	constexpr uintptr_t kRollbackCount = 0x845194;

	constexpr uintptr_t kGgpoSession = 0x4448858;
	constexpr uintptr_t kGgpoBackendVTable = 0x537910;
	constexpr uintptr_t kGgpoSpectatorBackendVTable = 0x5379f4;
	constexpr uintptr_t kGgpoPlayerEndpoints = 0xbd8;
	constexpr uintptr_t kGgpoSpectatorHostEndpoint = 0x450;
	constexpr uintptr_t kGgpoPlayerCount = 0x5d868;
	constexpr int kGgpoMostPlayers = 4;
	constexpr uintptr_t kGgpoEndpointUdp = 0x04;
	constexpr uintptr_t kGgpoEndpointQueue = 0x24;
	constexpr uintptr_t kGgpoEndpointRoundTrip = 0x860;
	constexpr uintptr_t kGgpoEndpointKbpsSent = 0x86c;
	constexpr uintptr_t kGgpoEndpointLocalBehind = 0x898;
	constexpr uintptr_t kGgpoEndpointRemoteBehind = 0x89c;
	constexpr uintptr_t kFnGgpoUdpLog = 0xa0e10;
	constexpr uintptr_t kFnGgpoProtocolLog = 0xa2270;
	constexpr uintptr_t kGgpoSyncPlayers = 0x59c;
	constexpr uintptr_t kGgpoSyncInputSize = 0x5a0;
	constexpr uintptr_t kGgpoLastConfirmedFrame = 0x5a8;
	constexpr uintptr_t kGgpoInputQueues = 0x5b4;
	constexpr size_t kGgpoInputQueueStride = 0x1654;
	constexpr uintptr_t kGgpoQueueInputs = 0x28;
	constexpr size_t kGgpoQueueInputStride = 0x2c;
	constexpr int kGgpoQueueSlots = 128;
	constexpr uintptr_t kGgpoInputBits = 0x08;
	constexpr uintptr_t kGgpoLocalConnectStatus = 0xbc4;
	constexpr uintptr_t kSpectatorOnEvent = 0x28;
	constexpr int kGgpoEventSynchronized = 0x3ea;
	constexpr int kGgpoEventRunning = 0x3eb;

	constexpr uintptr_t kFnStartSessionFromDescription = 0x1c20d0;
	constexpr uintptr_t kFnStartSpectatorSession = 0x1c1c60;
	constexpr uintptr_t kFnChooseRoomSession = 0x2b07f0;
	constexpr uintptr_t kFnAddSpectatorEndpoint = 0x9cee0;
	constexpr uintptr_t kFnRankMatchSetup = 0x2e0620;
	constexpr uintptr_t kFnMatchDispatch = 0x2e1030;
	constexpr int kMatchCaseRank = 4;
	constexpr int kMatchCasePlayer = 7;
	constexpr int kMatchCaseMainMenu = 0xb;
	constexpr uintptr_t kFnSessionTeardown = 0x1c0460;
	constexpr uintptr_t kOnlineMatchFlag = 0x1d595ed;
	constexpr uintptr_t kStageSyncOption = 0x5a8582;
	constexpr uint8_t kStageSyncOwn = 1;
	constexpr uintptr_t kNetworkStagePick = 0xc7e694;
	constexpr uintptr_t kStageOwnPick = 0x4448868;
	constexpr uintptr_t kAfterMatchState = 0x84380c;
	constexpr int kAfterMatchAwaitRematch = 0x401;
	constexpr int kBattleStartVsScreen = 2;
	constexpr uintptr_t kReplayRecording = 0x1d8a044;
	constexpr uintptr_t kReplaySaveResult = 0x1e8a15c;
	constexpr uintptr_t kFnApplyMatchBlock = 0x447dc0;

	constexpr uintptr_t kGgpoSynchronizing = 0x5d864;
	constexpr uintptr_t kGgpoSpectatorCount = 0x5d85c;
	constexpr uintptr_t kGgpoSpectatorEndpoints = 0xbdc;
	constexpr size_t kGgpoEndpointStride = 0x2e64;
	constexpr int kGgpoMostSpectators = 32;
	constexpr uintptr_t kGgpoEndpointSteamLow = 0x18;
	constexpr uintptr_t kGgpoEndpointSteamHigh = 0x1c;
	constexpr uintptr_t kGgpoEndpointState = 0x888;
	constexpr uintptr_t kGgpoEndpointSyncTimeout = 0x1f3c;
	constexpr uintptr_t kGgpoEndpointShutdownAt = 0x1f40;
	constexpr uint32_t kGgpoStateDisconnected = 3;
	constexpr uint32_t kGgpoStateRunning = 2;
	constexpr uintptr_t kGgpoEndpointPendingOutput = 0x1ea8;
	constexpr uint32_t kGgpoLocalPort = 0xe4a;
	constexpr uintptr_t kFnSendPendingOutput = 0xa2930;

	constexpr uintptr_t kFnSpectatorPoll = 0x9d880;
	constexpr uintptr_t kFnSpectatorSyncInput = 0x9d7f0;
	constexpr int kGgpoVTablePoll = 1;
	constexpr int kGgpoNotReady = 4;
	constexpr uintptr_t kSpectatorSynchronizing = 0x32b4;
	constexpr uintptr_t kSpectatorNextFrame = 0x32c0;
	constexpr uintptr_t kSpectatorInputs = 0x32c4;
	constexpr size_t kSpectatorInputBytes = 0x2c;
	constexpr int kSpectatorInputSlots = 64;

	constexpr size_t kSessionDescriptionSize = 0x280;
	constexpr size_t kSessionHolderSize = 0x908;
	constexpr uintptr_t kDescriptionGgpo = 0x00;
	constexpr uintptr_t kDescriptionLocalIndex = 0x08;
	constexpr uintptr_t kDescriptionLocalPort = 0x0c;
	constexpr uintptr_t kDescriptionFrameDelay = 0x10;
	constexpr uintptr_t kDescriptionPadSlot = 0x14;
	constexpr uintptr_t kDescriptionRemoteIp = 0x18;
	constexpr size_t kDescriptionRemoteIpBytes = 0x40;
	constexpr uintptr_t kDescriptionRemotePort = 0x58;
	constexpr uintptr_t kDescriptionRemoteSteamLow = 0x5c;
	constexpr uintptr_t kDescriptionRemoteSteamHigh = 0x60;
	constexpr int kDescriptionSpectatorIndex = 2;
	constexpr uintptr_t kInputDelayOption = 0x5a863a;
	constexpr uintptr_t kNetplayPadSlot = 0x7762e8;

	constexpr uintptr_t kMatchRecords = 0x1d59b50;
	constexpr size_t kMatchRecordsSize = 0x80;
	constexpr uintptr_t kMatchRecordStride = 0x40;
	constexpr uintptr_t kMatchRecordStage = 0x24;
	constexpr int kMatchRecordLocal = 0;
	constexpr int kMatchRecordRemote = 1;
	constexpr uintptr_t kMatchKind = 0x1d58fe4;
	constexpr int kMatchKindRank = 1;
	constexpr uintptr_t kMatchLocalSide = 0x1d59520;
	constexpr uintptr_t kMatchRemoteSide = 0x1d59524;
	constexpr int kMatchSpectatorSide = -1;
	constexpr uintptr_t kMatchSettingsWin = 0x1d58fd4;
	constexpr uintptr_t kMatchSeedCounter = 0x1d595e0;
	constexpr uintptr_t kRoomSettings = 0x1d59860;
	constexpr size_t kRoomSettingsSize = 0x248;
	constexpr uintptr_t kReadyFlags = 0x1d5962c;
	constexpr int kReadyPhasesPerSide = 3;
	constexpr int kReadyBattleStart = 1;
	constexpr int kReadyAfterMatch = 2;
	constexpr uintptr_t kBattleStartState = 0x843578;
	constexpr uintptr_t kBattleSessionState = 0x43fa8a0;
	constexpr uintptr_t kBattleCharaRecord = 0x776c78;
	constexpr size_t kBattleCharaRecordStride = 0x40;
	constexpr uintptr_t kBattleCharaColor = 0x758184;
	constexpr uintptr_t kBattleCharaExtra = 0x75818c;
	constexpr size_t kBattleCharaSlotStride = 0x1c;
	constexpr uintptr_t kBattleBgm = 0x4448fe4;
	constexpr uintptr_t kBattleRuleA = 0x4448798;
	constexpr uintptr_t kBattleRuleB = 0x444879c;
	constexpr uintptr_t kBattleRuleC = 0x4448768;
	constexpr uintptr_t kBattleRuleD = 0x444876c;

	constexpr uintptr_t kFnOnLobbyChatUpdate = 0x102100;
	constexpr uintptr_t kLobbyChatUpdateUser = 0x08;
	constexpr uintptr_t kLobbyChatUpdateFlags = 0x18;

	constexpr uintptr_t kSessionManager = 0x848f08;
	constexpr uintptr_t kSessionManagerLobbyId = 0x849031;

	constexpr uintptr_t kPlayerCardBlock = 0x5be248;
	constexpr size_t kPlayerCardBlockSize = 0x4000;

	constexpr uintptr_t kCardTitleText = 0x5be2b0;
	constexpr size_t kCardTitleTextBytes = 0x40;

	constexpr uintptr_t kCardPlateFrame = 0x5be3c8;
	constexpr uintptr_t kCardPlatePanel = 0x5be3cc;
	constexpr uintptr_t kCardPlateChara = 0x5be3d0;
	constexpr uintptr_t kCardIp = 0x5be3d4;
	constexpr uintptr_t kCardTitleId = 0x5be3d8;
	constexpr uintptr_t kCardPlateBase = 0x5be3dc;

	constexpr uintptr_t kTitleWords = 0x5ac967;
	constexpr size_t kTitleWordStride = 0x21;
	constexpr int kTitleWordCount = 3;

	constexpr uintptr_t kOwnedFrameList = 0x5aaae7;
	constexpr uintptr_t kOwnedPanelList = 0x5aaee7;
	constexpr uintptr_t kOwnedCharaList = 0x5ab2e7;
	constexpr uintptr_t kOwnedBaseList = 0x5ab6e7;

	constexpr uintptr_t kOwnedFrameCount = 0x5ac9cd;
	constexpr uintptr_t kOwnedPanelCount = 0x5ac9d1;
	constexpr uintptr_t kOwnedCharaCount = 0x5ac9d5;
	constexpr uintptr_t kOwnedBaseCount = 0x5ac9d9;

	constexpr int kOwnedMax = 256;

	constexpr uintptr_t kColourEquipped = 0x5c007c;
	constexpr uintptr_t kColourUnlockBits = 0x5c0428;
	constexpr uintptr_t kColourSlots = 0x5c0a48;

	constexpr uintptr_t kColourSlotsLive = 0x4433ef0;
	constexpr uintptr_t kColourUnlockTable = 0x4447fb0;

	constexpr int kColourCharacterMax = 64;
	constexpr int kColourUnlockCharacterMax = 32;
	constexpr int kColourUnlockStride = 0x2f;

	constexpr int kColourSlotsPerCharacter = 8;
	constexpr int kColourEntriesPerSlot = 8;

	constexpr int kColourFreeBelow = 10;
	constexpr int kColourUnlockBitCount = 32;
}
