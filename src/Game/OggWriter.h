#pragma once

#include <string>

struct OggEncoderState;

class OggWriter
{
public:
	~OggWriter();

	OggWriter(const OggWriter&) = delete;
	OggWriter& operator=(const OggWriter&) = delete;
	OggWriter() = default;

	bool Begin(const std::string& path, int channels, int rate, char* status, int statusSize);
	bool Write(const short* interleaved, int frames);
	bool Finish();

	void Abort();

private:
	void Drain(bool flush);
	void Release();

	OggEncoderState* m_state = nullptr;
	int m_channels = 0;
};
