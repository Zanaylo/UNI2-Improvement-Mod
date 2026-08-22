// SF6-style frame meter. It captures one exchange, then freezes it until the next begins.
// Never change an endpoint without measuring it against the game's own advantage field at +0x28.

#pragma once

#include <cstdint>

namespace FrameMeter
{
	enum class State : uint8_t
	{
		None,
		Idle,
		Startup,
		Armor,
		Active,
		Recovery,
		Blockstun,
		Hitstun,
		Parry,

		Cancellable,

		Movement,
		Jump,
		AirMovement,

		Dodge
	};

	constexpr int kPlayers = 2;

	constexpr int kCapacity = 600;

	struct Frame
	{
		State state;

		bool projectileActive;

		bool teching;

		uint16_t invuln;

		bool airJumpOK;

		uint16_t pattern;
	};

	constexpr int kComboStops = 8;
	constexpr int kMinComboHits = 1;
	constexpr int kMaxComboHits = 99;

	struct AutoPauseConfig
	{
		bool player[kPlayers];

		bool onMoveStarts;
		bool onHit;

		bool onComboHits;
		int comboStop[kComboStops];

		bool onBlockedHits;
		int blockStop[kComboStops];

		bool onDummyRecord;

		bool reversalAfterRestart;

		bool reversalAfterCountdown;

		bool showP2Inputs;

		bool onDummyPlayback;

		int leadInMode;

		bool blockedAllowsGaps;

		bool resumeDelay;
		int resumeDelayFrames;

		bool delayOnMoveStarts;
		bool delayOnHitLands;
		bool delayOnComboReaches;
		bool delayOnBlockReaches;
		bool delayOnManualPause;
	};

	enum class PauseSource
	{
		MoveStarts,
		HitLands,
		ComboReaches,
		BlockReaches
	};

	enum LeadInMode
	{
		LeadIn_WarmUp = 0,
		LeadIn_Still = 1
	};

	constexpr int kMinResumeFrames = 5;
	constexpr int kMaxResumeFrames = 600;

	void SampleFromGameThread();
	void Reset();

	AutoPauseConfig GetAutoPause();
	void SetAutoPause(const AutoPauseConfig& config);

	int PackAutoPause(const AutoPauseConfig& config);
	AutoPauseConfig UnpackAutoPause(int packed);

	bool IsRecording();

	int GetLength();
	bool GetFrame(int player, int index, Frame& out);

	bool HasAdvantage();
	int GetAdvantage();

	bool GetAdvantageFor(int player, int& outAdvantage);

	bool GetAdvantageAfterRecoveryFor(int player, int& outAdvantage);

	bool WasTrade();

	void SetTraceEnabled(bool enabled);
	bool IsTraceEnabled();
	void ShutdownTrace();

	bool GetLastMove(int player, int& outStartup, int& outActive, int& outRecovery, int& outTotal);

	const char* GetActionName(int player);

	int GetComboCount(int player);

	bool GetCharaAndPattern(int player, int& outChara, int& outPattern);

	int GetBlockedRun(int player);
	int GetBlockedTotal(int player);

	int GetFlashFrames(int player);
	bool IsSuperFlashRunning();

	bool ConsumeMatchRestart();

	bool IsPlayerFree(int player);

	int GetTrailingRunLength(int player);

	const char* GetStateName(State state);
	uint32_t GetStateColor(State state);

	enum Marker
	{
		Marker_Projectile,
		Marker_Tech,

		Marker_FullInvuln,
		Marker_StrikeInvuln,
		Marker_ThrowInvuln,
		Marker_ProjectileInvuln,
		Marker_HeadInvuln,
		Marker_BodyInvuln,
		Marker_LegsInvuln,
		Marker_DiveInvuln,
		Marker_HighMidInvuln,
		Marker_LowMidInvuln,

		Marker_COUNT
	};

	constexpr int kFirstInvulnMarker = Marker_FullInvuln;

	uint16_t GetMarkerInvulnBit(Marker marker);

	const char* GetMarkerName(Marker marker);
	uint32_t GetMarkerColor(Marker marker);
}
