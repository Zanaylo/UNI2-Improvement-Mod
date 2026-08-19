#pragma once

#include <cstdint>

struct IDirect3DDevice9;
struct IDirect3DBaseTexture9;

namespace PaletteTexture
{

	constexpr unsigned kWidth = 256;
	constexpr unsigned kRows = 64;
	constexpr int kRowBytes = kWidth * 4;

	constexpr int kMaxSeen = 32;

	bool OnSetTexture(IDirect3DDevice9* device, unsigned stage, IDirect3DBaseTexture9* texture);

	void Forget();
	void OnDeviceLost();

	void ForgetMatchTextures();

	void ResetMatchEvidence();

	int GetGeneration();
	bool HasPristineRow(int index, unsigned row);

	int GetSeenCount();
	uintptr_t GetSeen(int index);
	int GetBindCount(int index);

	int GetFrameCount(int index);

	enum class OwnerKind : uint8_t
	{
		Unknown,
		Player,
		Effect,
		CharSelect,
		LobbyAvatar,
	};

	struct TextureOwner
	{
		OwnerKind kind = OwnerKind::Unknown;
		uint8_t players = 0;
		int16_t chara = -1;
	};

	constexpr uint8_t PlayerBit(int player) { return static_cast<uint8_t>(1 << player); }

	enum class Claim : uint8_t
	{
		Guess,
		Strong,
		Hand,
	};

	TextureOwner GetOwnerInfo(int index);
	Claim GetClaim(int index);
	bool IsOwnerByHand(int index);
	int FindByPointer(uintptr_t texture);

	void SetHeuristicEnabled(bool enabled);
	bool IsHeuristicEnabled();

	void AssignOwner(int index, TextureOwner owner, Claim claim);
	int FindOwned(TextureOwner owner);

	void Evict(int index);

	void NoteInUse(int index);

	int GetLastBoundAge(int index);
	bool WasBoundInMatch(int index);
	int GetFirstSeenFrame(int index);
	int GetFrameSerial();

	int GetOwner(int index);
	void SetOwner(int index, int player);
	int FindForPlayer(int player);

	bool GetMatchTextures(int& outFirst, int& outSecond);

	void OnFrame();

	void SetRowsSwapped(bool swapped);
	bool GetRowsSwapped();

	int GetRowGeneration();

	bool RowHasRgb(int index, unsigned row, const uint8_t* rgba);

	int GetRowForPlayer(int player);

	void Dump();

	int GetShapeCount();
	bool GetShape(int index, unsigned& width, unsigned& height, unsigned& format, int& count);

	bool ReadRow(int index, unsigned row, uint8_t* out);
	bool WriteRow(int index, unsigned row, const uint8_t* colors);

	bool ReadRowAsRgba(int index, unsigned row, uint8_t* out);

	bool ReadPristineRowAsRgba(int index, unsigned row, uint8_t* out);

	bool Restore(int index, unsigned row);
	int RestoreAll(int index);
	bool HasBackup(int index, unsigned row);

	bool WriteRowKeepingAlpha(int index, unsigned row, const uint8_t* colors);

	int WriteToAll(const uint8_t* colors);

	void SetSwapSides(bool swap);
	bool GetSwapSides();
}
