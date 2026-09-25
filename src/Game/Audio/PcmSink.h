#pragma once

class PcmSink
{
public:
	virtual ~PcmSink() = default;

	virtual bool Begin(int channels, int rate, char* status, int statusSize) = 0;
	virtual void Write(const short* interleaved, int frames) = 0;
};
