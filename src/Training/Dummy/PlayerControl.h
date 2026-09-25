#pragma once

#include "Training/Dummy/InputFrame.h"

#include <cstdint>

namespace PlayerControl
{
	using Input = InputFrame;

	enum Mode
	{
		Mode_Mine,
		Mode_Other,
		Mode_Both
	};

	constexpr int kMaxScript = 3600;

	bool Initialize();

	bool ReadInput(int player, Input& out);

	int GetDummySide();
	int GetHomeSide();

	void SetMode(Mode mode);
	Mode GetMode();

	void SetHeld(uint8_t lever, uint8_t buttons);
	void Tap(uint8_t lever, uint8_t buttons);

	void RunScript(int player, const Input* frames, int count);
	void StopScript(int player);
	bool IsScriptRunning(int player);
	int GetScriptFrame(int player);

	void Release();
	bool IsDriving();

	void Calibrate();
	bool IsCalibrating();

	void KeepAlive();

	void Update();

	void OnFrameUpdate();

	void OnPadFetched(void* out, int player);

	const char* GetStatus();
}
