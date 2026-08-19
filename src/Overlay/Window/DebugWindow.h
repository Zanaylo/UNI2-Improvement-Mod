// The reverse-engineering scratchpad: scanners, probes and readouts. Off by default, gated on the ini.

#pragma once

#include "Game/HitboxData.h"
#include "Game/PlayerState.h"
#include "Overlay/Window/IWindow.h"

#include <cstdint>

class DebugWindow : public IWindow
{
public:
	static constexpr int kMaxSearchResults = 16;

	DebugWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void Draw() override;

private:
	enum class FilterMode
	{
		Changed,
		Unchanged,
		Increased,
		Decreased
	};

	static constexpr int kDwordCount = 0xba4 / 4;

	void RefreshEntities();
	void DumpToLog();
	void BeginCapture();
	void CaptureFrame();
	void CaptureSummary();
	void DrawEntitySection();
	void DrawCharaSlotsSection();
	void DrawHitboxSection();
	void DrawTransformSection();
	void DrawDiffSearchSection();
	void DrawPointerSection();
	void DrawScannerSection();
	void DrawPointerScan();
	void DrawPlayerStateSection();
	void DrawMeterComparisonSection();
	void DrawRecorderControls();
	void DrawStopTimeSection();
	void DrawStructSection();
	void DrawSaveSection();
	void DrawPaletteOwnerSection();

	void UpdateComparison();
	void CaptureComparison();

	void TakeSnapshot(bool resetCandidates);
	void ApplyFilter(FilterMode mode);
	int CountCandidates() const;

	void* GetSelectedEntity() const;

	int m_paletteWritten[2] = { -1, -1 };
	bool m_paletteBackedUp[2] = {};
	uint8_t m_paletteBackup[2][1024] = {};

	int m_selectedEntity;
	int m_structFirstOffset;
	int m_structRowCount;

	bool m_hasSnapshot;
	uint32_t m_snapshot[kDwordCount];
	bool m_candidate[kDwordCount];

	uintptr_t m_followOffset;
	int m_followRows;

	uintptr_t m_nameOffset = 0xacc;

	HitboxData::ScanResult m_lastScan = {};

	int m_captureRemaining = 0;
	int m_captureFrame = 0;
	int m_capturePeakActive = 0;

	static constexpr int kComparisonHistory = 12;

	struct ComparisonSample
	{
		int meterStartup;
		int meterTotal;
		int meterAdvantage;
		bool meterHasAdvantage;
		bool knockdown;
		int gameStartup;
		int gameTotal;
		int gameAdvantage;
	};

	ComparisonSample m_comparisons[kComparisonHistory] = {};
	int m_comparisonCount = 0;
	int m_comparisonPlayer = 0;
	bool m_hasLastGameDisplay = false;
	bool m_wasRecording = false;
	PlayerState::FrameDisplay m_lastGameDisplay = {};

	bool m_forceEffectTint = false;
	float m_forceEffectRgb[3] = { 1.0f, 0.0f, 1.0f };
	int m_forceEffectEntry = -1;

	int m_stopTimeFrames = 60;
	bool m_recordDeltas = true;
	int m_scanValue = 0;
	char m_pointerTarget[16] = {};
};
