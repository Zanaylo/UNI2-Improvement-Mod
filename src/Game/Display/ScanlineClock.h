#pragma once

#include "Game/Display/DisplayGrid.h"

#include <Windows.h>

#include <cstdint>

class ScanlineClock
{
public:
	enum class Reading
	{
		Anchored,
		Unusable,
		Failed,
	};

	ScanlineClock(int64_t frequency, int64_t slowestReadTicks);
	~ScanlineClock();

	ScanlineClock(const ScanlineClock&) = delete;
	ScanlineClock& operator=(const ScanlineClock&) = delete;

	bool Open(HWND window);
	void Close();

	bool IsOpenOn(HMONITOR monitor) const;
	Reading Sample();
	bool HasAnchor() const;

	const DisplayGrid& Grid() const;
	double RefreshHz() const;

private:
	bool ReadMode(const wchar_t* gdiDeviceName);
	bool OpenAdapter(const wchar_t* gdiDeviceName);
	void FindFirstAnchor();

	int64_t m_frequency;
	int64_t m_slowestReadTicks;
	HMONITOR m_monitor = nullptr;
	uint32_t m_adapter = 0;
	uint32_t m_source = 0;
	ScanoutMode m_mode = {};
	DisplayGrid m_grid = {};
	double m_refreshHz = 0.0;
};
