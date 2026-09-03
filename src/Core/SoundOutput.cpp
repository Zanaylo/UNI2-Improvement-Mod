#include "Core/SoundOutput.h"

#include <Windows.h>
#include <mmsystem.h>

namespace {

constexpr int kBitsPerSample = 16;
constexpr int kMaxChannels = 2;
constexpr int kMinRate = 8000;
constexpr int kMaxRate = 192000;

HWAVEOUT g_device = nullptr;
WAVEHDR g_header = {};
std::vector<short> g_samples;

bool SettingsAreUsable(size_t frames, int channels, int rate)
{
	if (frames == 0 || channels < 1 || channels > kMaxChannels)
		return false;

	return rate >= kMinRate && rate <= kMaxRate;
}

WAVEFORMATEX FormatFor(int channels, int rate)
{
	WAVEFORMATEX format = {};

	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = static_cast<WORD>(channels);
	format.nSamplesPerSec = static_cast<DWORD>(rate);
	format.wBitsPerSample = kBitsPerSample;
	format.nBlockAlign = static_cast<WORD>(channels * (kBitsPerSample / 8));
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	return format;
}

}

bool SoundOutput::Play(std::vector<short> samples, int channels, int rate)
{
	Stop();

	if (!SettingsAreUsable(samples.size(), channels, rate))
		return false;

	const WAVEFORMATEX format = FormatFor(channels, rate);

	if (waveOutOpen(&g_device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
	{
		g_device = nullptr;
		return false;
	}

	g_samples = std::move(samples);

	g_header = {};
	g_header.lpData = reinterpret_cast<LPSTR>(g_samples.data());
	g_header.dwBufferLength = static_cast<DWORD>(g_samples.size() * sizeof(short));

	if (waveOutPrepareHeader(g_device, &g_header, sizeof(g_header)) != MMSYSERR_NOERROR)
	{
		Stop();
		return false;
	}

	if (waveOutWrite(g_device, &g_header, sizeof(g_header)) != MMSYSERR_NOERROR)
	{
		Stop();
		return false;
	}

	return true;
}

void SoundOutput::Stop()
{
	if (g_device == nullptr)
		return;

	waveOutReset(g_device);

	if ((g_header.dwFlags & WHDR_PREPARED) != 0)
		waveOutUnprepareHeader(g_device, &g_header, sizeof(g_header));

	waveOutClose(g_device);

	g_device = nullptr;
	g_header = {};
	g_samples.clear();
}

bool SoundOutput::IsPlaying()
{
	return g_device != nullptr && (g_header.dwFlags & WHDR_DONE) == 0;
}
