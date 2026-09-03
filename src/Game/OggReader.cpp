#include "Game/OggReader.h"

#include "Game/PcmSink.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr int kHeaderPackets = 3;
constexpr int kBlockFrames = 4096;
constexpr int kMaxChannels = 2;

short ToShort(float value)
{
	const float scaled = value * 32767.0f;

	if (scaled >= 32767.0f)
		return 32767;

	if (scaled <= -32768.0f)
		return -32768;

	return static_cast<short>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

class Decoder
{
public:
	Decoder()
	{
		ogg_sync_init(&m_sync);
		vorbis_info_init(&m_info);
		vorbis_comment_init(&m_comment);
	}

	~Decoder()
	{
		if (m_playing)
		{
			vorbis_block_clear(&m_block);
			vorbis_dsp_clear(&m_dsp);
		}

		if (m_streaming)
			ogg_stream_clear(&m_stream);

		vorbis_comment_clear(&m_comment);
		vorbis_info_clear(&m_info);
		ogg_sync_clear(&m_sync);
	}

	Decoder(const Decoder&) = delete;
	Decoder& operator=(const Decoder&) = delete;

	bool Run(const uint8_t* data, size_t size, PcmSink& sink, char* status, int statusSize);

private:
	bool Feed(const uint8_t* data, size_t size);
	bool TakePacket(const ogg_packet& packet, PcmSink& sink, char* status, int statusSize);
	bool Start(PcmSink& sink, char* status, int statusSize);
	void Drain(PcmSink& sink);

	ogg_sync_state m_sync = {};
	ogg_stream_state m_stream = {};
	vorbis_info m_info = {};
	vorbis_comment m_comment = {};
	vorbis_dsp_state m_dsp = {};
	vorbis_block m_block = {};

	std::vector<short> m_scratch;

	int m_headers = 0;
	bool m_streaming = false;
	bool m_playing = false;
	bool m_wrote = false;
};

bool Decoder::Feed(const uint8_t* data, size_t size)
{
	char* const buffer = ogg_sync_buffer(&m_sync, static_cast<long>(size));

	if (buffer == nullptr)
		return false;

	memcpy(buffer, data, size);
	return ogg_sync_wrote(&m_sync, static_cast<long>(size)) == 0;
}

bool Decoder::Start(PcmSink& sink, char* status, int statusSize)
{
	if (m_info.channels < 1 || m_info.channels > kMaxChannels)
	{
		sprintf_s(status, statusSize, "has %d channels; only mono and stereo can be played",
			m_info.channels);
		return false;
	}

	if (vorbis_synthesis_init(&m_dsp, &m_info) != 0)
	{
		strncpy_s(status, statusSize, "is an Ogg Vorbis stream that could not be opened", _TRUNCATE);
		return false;
	}

	vorbis_block_init(&m_dsp, &m_block);
	m_playing = true;

	m_scratch.resize(static_cast<size_t>(kBlockFrames) * m_info.channels);

	return sink.Begin(m_info.channels, m_info.rate, status, statusSize);
}

void Decoder::Drain(PcmSink& sink)
{
	float** pcm = nullptr;
	int available = 0;

	while ((available = vorbis_synthesis_pcmout(&m_dsp, &pcm)) > 0)
	{
		const int take = available < kBlockFrames ? available : kBlockFrames;

		for (int frame = 0; frame < take; ++frame)
		{
			for (int channel = 0; channel < m_info.channels; ++channel)
				m_scratch[static_cast<size_t>(frame) * m_info.channels + channel] =
					ToShort(pcm[channel][frame]);
		}

		sink.Write(m_scratch.data(), take);
		vorbis_synthesis_read(&m_dsp, take);

		m_wrote = true;
	}
}

bool Decoder::TakePacket(const ogg_packet& packet, PcmSink& sink, char* status, int statusSize)
{
	ogg_packet copy = packet;

	if (m_headers < kHeaderPackets)
	{
		if (vorbis_synthesis_headerin(&m_info, &m_comment, &copy) != 0)
		{
			strncpy_s(status, statusSize, "is not an Ogg Vorbis stream", _TRUNCATE);
			return false;
		}

		++m_headers;

		if (m_headers < kHeaderPackets)
			return true;

		return Start(sink, status, statusSize);
	}

	if (vorbis_synthesis(&m_block, &copy) == 0)
		vorbis_synthesis_blockin(&m_dsp, &m_block);

	Drain(sink);
	return true;
}

bool Decoder::Run(const uint8_t* data, size_t size, PcmSink& sink, char* status, int statusSize)
{
	if (!Feed(data, size))
	{
		strncpy_s(status, statusSize, "could not be read", _TRUNCATE);
		return false;
	}

	ogg_page page = {};

	while (ogg_sync_pageout(&m_sync, &page) == 1)
	{
		if (!m_streaming)
		{
			ogg_stream_init(&m_stream, ogg_page_serialno(&page));
			m_streaming = true;
		}

		if (ogg_stream_pagein(&m_stream, &page) != 0)
			continue;

		ogg_packet packet = {};

		while (ogg_stream_packetout(&m_stream, &packet) == 1)
		{
			if (!TakePacket(packet, sink, status, statusSize))
				return false;
		}
	}

	if (!m_wrote)
	{
		strncpy_s(status, statusSize, "holds no Ogg Vorbis audio", _TRUNCATE);
		return false;
	}

	return true;
}

}

bool OggReader::Decode(const uint8_t* data, size_t size, PcmSink& sink, char* status,
	int statusSize)
{
	if (data == nullptr || size == 0)
	{
		strncpy_s(status, statusSize, "is empty", _TRUNCATE);
		return false;
	}

	Decoder decoder;
	return decoder.Run(data, size, sink, status, statusSize);
}
