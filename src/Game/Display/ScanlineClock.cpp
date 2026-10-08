#include "Game/Display/ScanlineClock.h"

#include <winternl.h>
#include <d3dkmthk.h>

#include <vector>

#pragma comment(lib, "gdi32.lib")

namespace {

constexpr NTSTATUS kStatusSuccess = 0;
constexpr int kFirstAnchorTries = 400;
constexpr int64_t kHalf = 2;

int64_t Now()
{
	LARGE_INTEGER value = {};
	QueryPerformanceCounter(&value);
	return value.QuadPart;
}

bool SourceNameIs(const DISPLAYCONFIG_PATH_INFO& path, const wchar_t* gdiDeviceName)
{
	DISPLAYCONFIG_SOURCE_DEVICE_NAME source = {};
	source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
	source.header.size = sizeof(source);
	source.header.adapterId = path.sourceInfo.adapterId;
	source.header.id = path.sourceInfo.id;

	return DisplayConfigGetDeviceInfo(&source.header) == ERROR_SUCCESS &&
		wcscmp(source.viewGdiDeviceName, gdiDeviceName) == 0;
}

bool FindSignal(const wchar_t* gdiDeviceName, DISPLAYCONFIG_VIDEO_SIGNAL_INFO& signal)
{
	UINT32 pathCount = 0;
	UINT32 modeCount = 0;
	if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
		return false;

	std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
	std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
	if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) !=
		ERROR_SUCCESS)
	{
		return false;
	}

	for (UINT32 i = 0; i < pathCount; ++i)
	{
		const UINT32 index = paths[i].targetInfo.modeInfoIdx;
		if (index >= modeCount || modes[index].infoType != DISPLAYCONFIG_MODE_INFO_TYPE_TARGET)
			continue;

		if (!SourceNameIs(paths[i], gdiDeviceName))
			continue;

		signal = modes[index].targetMode.targetVideoSignalInfo;
		return true;
	}

	return false;
}

}

ScanlineClock::ScanlineClock(int64_t frequency, int64_t slowestReadTicks)
	: m_frequency(frequency)
	, m_slowestReadTicks(slowestReadTicks)
{
}

ScanlineClock::~ScanlineClock()
{
	Close();
}

bool ScanlineClock::Open(HWND window)
{
	Close();

	if (window == nullptr)
		return false;

	const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEXW info = {};
	info.cbSize = sizeof(info);
	if (monitor == nullptr || !GetMonitorInfoW(monitor, &info))
		return false;

	if (!ReadMode(info.szDevice) || !OpenAdapter(info.szDevice))
		return false;

	m_monitor = monitor;
	FindFirstAnchor();
	return true;
}

void ScanlineClock::Close()
{
	if (m_adapter != 0)
	{
		D3DKMT_CLOSEADAPTER close = {};
		close.hAdapter = m_adapter;
		D3DKMTCloseAdapter(&close);
	}

	m_monitor = nullptr;
	m_adapter = 0;
	m_source = 0;
	m_mode = {};
	m_grid = {};
	m_refreshHz = 0.0;
}

bool ScanlineClock::IsOpenOn(HMONITOR monitor) const
{
	return m_adapter != 0 && m_monitor == monitor;
}

ScanlineClock::Reading ScanlineClock::Sample()
{
	if (m_adapter == 0)
		return Reading::Failed;

	D3DKMT_GETSCANLINE scan = {};
	scan.hAdapter = m_adapter;
	scan.VidPnSourceId = m_source;

	const int64_t before = Now();
	const NTSTATUS status = D3DKMTGetScanLine(&scan);
	const int64_t after = Now();

	if (status != kStatusSuccess)
		return Reading::Failed;

	int64_t anchor = 0;
	const ScanlineReading reading = { before, after, scan.ScanLine, scan.InVerticalBlank != FALSE };
	if (!Scanout::Anchor(reading, m_mode, m_slowestReadTicks, anchor))
		return Reading::Unusable;

	m_grid = { anchor, m_mode.refreshTicks };
	return Reading::Anchored;
}

bool ScanlineClock::HasAnchor() const
{
	return m_grid.refreshTicks > 0;
}

const DisplayGrid& ScanlineClock::Grid() const
{
	return m_grid;
}

double ScanlineClock::RefreshHz() const
{
	return m_refreshHz;
}

bool ScanlineClock::ReadMode(const wchar_t* gdiDeviceName)
{
	DISPLAYCONFIG_VIDEO_SIGNAL_INFO signal = {};
	if (!FindSignal(gdiDeviceName, signal))
		return false;

	const DISPLAYCONFIG_RATIONAL& rate = signal.vSyncFreq;
	if (rate.Numerator == 0 || rate.Denominator == 0 || signal.totalSize.cy == 0 ||
		signal.scanLineOrdering != DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE)
	{
		return false;
	}

	const int64_t refreshTicks = (m_frequency * rate.Denominator + rate.Numerator / kHalf) / rate.Numerator;
	m_mode = { refreshTicks, signal.totalSize.cy };
	m_refreshHz = static_cast<double>(rate.Numerator) / rate.Denominator;
	return true;
}

bool ScanlineClock::OpenAdapter(const wchar_t* gdiDeviceName)
{
	D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open = {};
	wcscpy_s(open.DeviceName, gdiDeviceName);

	if (D3DKMTOpenAdapterFromGdiDisplayName(&open) != kStatusSuccess)
		return false;

	m_adapter = open.hAdapter;
	m_source = open.VidPnSourceId;
	return true;
}

void ScanlineClock::FindFirstAnchor()
{
	for (int attempt = 0; attempt < kFirstAnchorTries && !HasAnchor(); ++attempt)
		Sample();
}
