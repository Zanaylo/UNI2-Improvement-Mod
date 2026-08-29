#include "Game/OggWriter.h"

#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

struct OggEncoderState
{
	vorbis_info info;
	vorbis_comment comment;
	vorbis_dsp_state dsp;
	vorbis_block block;
	ogg_stream_state stream;
	FILE* file;
	bool blockReady;
};

namespace {

constexpr float kQuality = 0.5f;
constexpr int kChunkFrames = 1024;
constexpr int kMinRate = 8000;
constexpr int kMaxRate = 192000;
constexpr int kMaxChannels = 2;

uint32_t SerialFor(const std::string& path)
{
	uint32_t hash = 2166136261u;

	for (const char c : path)
	{
		hash ^= static_cast<uint8_t>(c);
		hash *= 16777619u;
	}

	return hash & 0x7fffffffu;
}

void WritePage(FILE* file, const ogg_page& page)
{
	fwrite(page.header, 1, page.header_len, file);
	fwrite(page.body, 1, page.body_len, file);
}

void FlushPages(OggEncoderState& encoder)
{
	ogg_page page = {};

	while (ogg_stream_flush(&encoder.stream, &page) != 0)
		WritePage(encoder.file, page);
}

bool SettingsAreUsable(int channels, int rate)
{
	return channels >= 1 && channels <= kMaxChannels && rate >= kMinRate && rate <= kMaxRate;
}

}

OggWriter::~OggWriter()
{
	Release();
}

void OggWriter::Release()
{
	if (m_state == nullptr)
	{
		m_channels = 0;
		return;
	}

	ogg_stream_clear(&m_state->stream);

	if (m_state->blockReady)
		vorbis_block_clear(&m_state->block);

	vorbis_dsp_clear(&m_state->dsp);
	vorbis_comment_clear(&m_state->comment);
	vorbis_info_clear(&m_state->info);

	if (m_state->file != nullptr)
		fclose(m_state->file);

	delete m_state;
	m_state = nullptr;
	m_channels = 0;
}

bool OggWriter::Begin(const std::string& path, int channels, int rate, char* status,
	int statusSize)
{
	Release();

	if (!SettingsAreUsable(channels, rate))
	{
		sprintf_s(status, statusSize, "%d channel(s) at %d Hz is not something the encoder takes",
			channels, rate);
		return false;
	}

	OggEncoderState* encoder = new OggEncoderState();
	encoder->file = nullptr;
	encoder->blockReady = false;

	vorbis_info_init(&encoder->info);

	if (vorbis_encode_init_vbr(&encoder->info, channels, rate, kQuality) != 0)
	{
		vorbis_info_clear(&encoder->info);
		delete encoder;
		strncpy_s(status, statusSize, "the encoder refused those settings", _TRUNCATE);
		return false;
	}

	vorbis_comment_init(&encoder->comment);
	vorbis_comment_add_tag(&encoder->comment, "ENCODER", "UNI2 Improvement Mod");
	vorbis_analysis_init(&encoder->dsp, &encoder->info);
	vorbis_block_init(&encoder->dsp, &encoder->block);
	encoder->blockReady = true;

	ogg_stream_init(&encoder->stream, static_cast<int>(SerialFor(path)));

	m_state = encoder;
	m_channels = channels;

	if (fopen_s(&encoder->file, path.c_str(), "wb") != 0 || encoder->file == nullptr)
	{
		Release();
		strncpy_s(status, statusSize, "the converted copy could not be written", _TRUNCATE);
		return false;
	}

	ogg_packet header = {};
	ogg_packet headerComment = {};
	ogg_packet headerCode = {};

	vorbis_analysis_headerout(&encoder->dsp, &encoder->comment, &header, &headerComment,
		&headerCode);

	ogg_stream_packetin(&encoder->stream, &header);
	ogg_stream_packetin(&encoder->stream, &headerComment);
	ogg_stream_packetin(&encoder->stream, &headerCode);

	FlushPages(*encoder);
	return true;
}

void OggWriter::Drain(bool flush)
{
	ogg_page page = {};
	ogg_packet packet = {};

	while (vorbis_analysis_blockout(&m_state->dsp, &m_state->block) == 1)
	{
		vorbis_analysis(&m_state->block, nullptr);
		vorbis_bitrate_addblock(&m_state->block);

		while (vorbis_bitrate_flushpacket(&m_state->dsp, &packet) != 0)
		{
			ogg_stream_packetin(&m_state->stream, &packet);

			while (ogg_stream_pageout(&m_state->stream, &page) != 0)
			{
				WritePage(m_state->file, page);

				if (ogg_page_eos(&page) != 0)
					return;
			}
		}
	}

	if (flush)
		FlushPages(*m_state);
}

bool OggWriter::Write(const short* interleaved, int frames)
{
	if (m_state == nullptr || frames <= 0)
		return false;

	int done = 0;

	while (done < frames)
	{
		const int take = frames - done < kChunkFrames ? frames - done : kChunkFrames;
		float** target = vorbis_analysis_buffer(&m_state->dsp, take);

		for (int channel = 0; channel < m_channels; ++channel)
		{
			const short* source = interleaved + (static_cast<size_t>(done) * m_channels) + channel;

			for (int i = 0; i < take; ++i)
				target[channel][i] = source[static_cast<size_t>(i) * m_channels] / 32768.0f;
		}

		vorbis_analysis_wrote(&m_state->dsp, take);
		Drain(false);

		done += take;
	}

	return true;
}

bool OggWriter::Finish()
{
	if (m_state == nullptr)
		return false;

	vorbis_analysis_wrote(&m_state->dsp, 0);
	Drain(true);

	Release();
	return true;
}

void OggWriter::Abort()
{
	Release();
}
