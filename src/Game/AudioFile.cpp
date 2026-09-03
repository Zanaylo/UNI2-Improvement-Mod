#include "Game/AudioFile.h"

#include "Core/logger.h"
#include "Game/OggReader.h"
#include "Game/OggWriter.h"
#include "Game/PcmSink.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#define MINIMP3_NO_STDIO
#pragma warning(push)
#pragma warning(disable : 4244)
#include "minimp3_ex.h"
#pragma warning(pop)

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr int kHeaderBytes = 64;
constexpr int kMinWavBytes = 44;
constexpr int kMaxChannels = 2;
constexpr int kBlockFrames = 4096;
constexpr uint16_t kFormatPcm = 1;
constexpr uint16_t kFormatFloat = 3;
constexpr uint16_t kFormatExtensible = 0xfffe;

bool ReadHeader(const std::string& path, uint8_t* out, int size, int& outRead)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	outRead = static_cast<int>(fread(out, 1, size, handle));
	fclose(handle);
	return true;
}

bool Tag(const uint8_t* header, int size, int at, const char* tag)
{
	const int length = static_cast<int>(strlen(tag));

	if (at + length > size)
		return false;

	return memcmp(header + at, tag, length) == 0;
}

bool Contains(const uint8_t* header, int size, const char* tag)
{
	const int length = static_cast<int>(strlen(tag));

	for (int at = 0; at + length <= size; ++at)
	{
		if (memcmp(header + at, tag, length) == 0)
			return true;
	}

	return false;
}

bool IsWav(const uint8_t* header, int size)
{
	return Tag(header, size, 0, "RIFF") && Tag(header, size, 8, "WAVE");
}

bool IsMp3(const uint8_t* header, int size)
{
	if (Tag(header, size, 0, "ID3"))
		return true;

	for (int at = 0; at + 1 < size; ++at)
	{
		if (header[at] != 0xff)
			continue;

		if ((header[at + 1] & 0xe0) == 0xe0)
			return true;

		return false;
	}

	return false;
}

std::vector<uint8_t> ReadWhole(const std::string& path)
{
	std::vector<uint8_t> data;

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return data;

	fseek(handle, 0, SEEK_END);
	const long size = ftell(handle);
	fseek(handle, 0, SEEK_SET);

	if (size > 0)
	{
		data.resize(static_cast<size_t>(size));

		if (fread(data.data(), 1, data.size(), handle) != data.size())
			data.clear();
	}

	fclose(handle);
	return data;
}

uint32_t ReadU32(const uint8_t* at)
{
	return static_cast<uint32_t>(at[0]) | (static_cast<uint32_t>(at[1]) << 8) |
		(static_cast<uint32_t>(at[2]) << 16) | (static_cast<uint32_t>(at[3]) << 24);
}

uint16_t ReadU16(const uint8_t* at)
{
	return static_cast<uint16_t>(static_cast<uint16_t>(at[0]) | (static_cast<uint16_t>(at[1]) << 8));
}

struct WavView
{
	const uint8_t* data;
	size_t bytes;
	int channels;
	int rate;
	int bits;
	bool isFloat;
};

void ReadFormatChunk(const uint8_t* body, uint32_t size, WavView& out, bool& outKnown)
{
	if (size < 16)
		return;

	uint16_t tag = ReadU16(body);
	out.channels = ReadU16(body + 2);
	out.rate = static_cast<int>(ReadU32(body + 4));
	out.bits = ReadU16(body + 14);

	if (tag == kFormatExtensible && size >= 26)
		tag = ReadU16(body + 24);

	out.isFloat = tag == kFormatFloat;
	outKnown = tag == kFormatPcm || tag == kFormatFloat;
}

bool WalkChunks(const std::vector<uint8_t>& file, WavView& out)
{
	bool known = false;
	size_t at = 12;

	while (at + 8 <= file.size())
	{
		const uint8_t* chunk = file.data() + at;
		const uint32_t size = ReadU32(chunk + 4);
		const size_t body = at + 8;

		if (size > file.size() - body)
			break;

		if (memcmp(chunk, "fmt ", 4) == 0)
			ReadFormatChunk(file.data() + body, size, out, known);
		else if (memcmp(chunk, "data", 4) == 0)
		{
			out.data = file.data() + body;
			out.bytes = size;
		}

		at = body + size + (size & 1);
	}

	return known;
}

bool DepthIsReadable(const WavView& wav)
{
	if (wav.isFloat)
		return wav.bits == 32;

	return wav.bits == 8 || wav.bits == 16 || wav.bits == 24 || wav.bits == 32;
}

bool ParseWav(const std::vector<uint8_t>& file, WavView& out, char* status, int statusSize)
{
	memset(&out, 0, sizeof(out));

	if (file.size() < kMinWavBytes)
	{
		strncpy_s(status, statusSize, "is too short to be a WAV", _TRUNCATE);
		return false;
	}

	if (!WalkChunks(file, out))
	{
		strncpy_s(status, statusSize, "is a WAV the mod cannot read - save it as plain PCM",
			_TRUNCATE);
		return false;
	}

	if (out.data == nullptr || out.bytes == 0)
	{
		strncpy_s(status, statusSize, "is a WAV with no audio in it", _TRUNCATE);
		return false;
	}

	if (out.channels < 1 || out.channels > kMaxChannels)
	{
		sprintf_s(status, statusSize, "has %d channels; the game takes mono or stereo",
			out.channels);
		return false;
	}

	if (!DepthIsReadable(out))
	{
		sprintf_s(status, statusSize, "is %d bit audio the mod cannot read", out.bits);
		return false;
	}

	return true;
}

short SampleAt(const WavView& wav, size_t index)
{
	const int stride = wav.bits / 8;
	const uint8_t* at = wav.data + index * stride;

	if (wav.isFloat)
	{
		float value = 0.0f;
		memcpy(&value, at, sizeof(value));

		const float scaled = value * 32767.0f;

		if (scaled >= 32767.0f)
			return 32767;

		if (scaled <= -32768.0f)
			return -32768;

		return static_cast<short>(scaled);
	}

	if (wav.bits == 8)
		return static_cast<short>((static_cast<int>(*at) - 128) << 8);

	if (wav.bits == 16)
		return static_cast<short>(ReadU16(at));

	if (wav.bits == 24)
		return static_cast<short>(ReadU16(at + 1));

	return static_cast<short>(ReadU16(at + 2));
}

bool DecodeWav(const std::vector<uint8_t>& file, PcmSink& sink, char* status, int statusSize)
{
	WavView wav = {};

	if (!ParseWav(file, wav, status, statusSize))
		return false;

	if (!sink.Begin(wav.channels, wav.rate, status, statusSize))
		return false;

	const size_t total = wav.bytes / (wav.bits / 8);
	const size_t frames = total / wav.channels;

	std::vector<short> block(kBlockFrames * static_cast<size_t>(wav.channels));

	for (size_t done = 0; done < frames;)
	{
		const size_t take = frames - done < kBlockFrames ? frames - done : kBlockFrames;

		for (size_t i = 0; i < take * wav.channels; ++i)
			block[i] = SampleAt(wav, (done * wav.channels) + i);

		sink.Write(block.data(), static_cast<int>(take));
		done += take;
	}

	LOG("AudioFile: %d Hz, %d channel(s), %d bit%s", wav.rate, wav.channels, wav.bits,
		wav.isFloat ? " float" : "");

	return true;
}

struct Gapless
{
	int start;
	int head;
	int tail;
};

Gapless ReadGapless(const uint8_t* data, int size)
{
	Gapless out = { 0, 0, 0 };

	mp3dec_t probe = {};
	mp3dec_init(&probe);

	mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME] = {};
	mp3dec_frame_info_t info = {};

	mp3dec_decode_frame(&probe, data, size, pcm, &info);

	if (info.frame_bytes <= 0)
		return out;

	uint32_t frames = 0;
	int delay = 0;
	int padding = 0;

	if (mp3dec_check_vbrtag(data + info.frame_offset, info.frame_bytes, &frames, &delay,
		&padding) != 1)
	{
		return out;
	}

	out.start = info.frame_offset + info.frame_bytes;
	out.head = delay > 0 ? delay : 0;
	out.tail = padding > 0 ? padding : 0;

	LOG("AudioFile: the encoder tag says %d sample(s) of lead-in and %d of tail padding", out.head,
		out.tail);

	return out;
}

class GaplessFeed
{
public:
	GaplessFeed(const Gapless& trim, int channels)
		: m_channels(channels), m_skip(trim.head), m_tail(trim.tail)
	{
	}

	void Push(const short* samples, int frames, PcmSink& sink)
	{
		const int kept = DropHead(samples, frames);

		if (kept <= 0)
			return;

		m_held.insert(m_held.end(), samples, samples + (static_cast<size_t>(kept) * m_channels));

		const int ready = static_cast<int>(m_held.size() / m_channels) - m_tail;

		if (ready <= 0)
			return;

		sink.Write(m_held.data(), ready);
		m_held.erase(m_held.begin(), m_held.begin() + (static_cast<size_t>(ready) * m_channels));
	}

private:
	int DropHead(const short*& samples, int frames)
	{
		if (m_skip <= 0)
			return frames;

		const int drop = m_skip < frames ? m_skip : frames;

		m_skip -= drop;
		samples += static_cast<size_t>(drop) * m_channels;
		return frames - drop;
	}

	std::vector<short> m_held;
	int m_channels;
	int m_skip;
	int m_tail;
};

bool DecodeMp3(const std::vector<uint8_t>& file, PcmSink& sink, char* status, int statusSize)
{
	const uint8_t* data = file.data();
	size_t size = file.size();
	mp3dec_skip_id3(&data, &size);

	if (size == 0)
	{
		strncpy_s(status, statusSize, "holds no MP3 audio", _TRUNCATE);
		return false;
	}

	const Gapless gapless = ReadGapless(data, static_cast<int>(size));

	mp3dec_t decoder = {};
	mp3dec_init(&decoder);

	mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME] = {};
	mp3dec_frame_info_t info = {};

	std::unique_ptr<GaplessFeed> feed;
	int offset = gapless.start;
	int channels = 0;
	int rate = 0;

	while (offset < static_cast<int>(size))
	{
		const int decoded = mp3dec_decode_frame(&decoder, data + offset,
			static_cast<int>(size) - offset, pcm, &info);

		if (info.frame_bytes == 0)
			break;

		offset += info.frame_bytes;

		if (decoded == 0)
			continue;

		if (feed == nullptr)
		{
			channels = info.channels;
			rate = info.hz;

			if (!sink.Begin(channels, rate, status, statusSize))
				return false;

			feed.reset(new GaplessFeed(gapless, channels));

			LOG("AudioFile: %d Hz, %d channel(s), %d kbps", rate, channels, info.bitrate_kbps);
		}

		if (info.channels != channels || info.hz != rate)
			continue;

		feed->Push(pcm, decoded, sink);
	}

	if (feed == nullptr)
	{
		strncpy_s(status, statusSize, "holds no MP3 audio", _TRUNCATE);
		return false;
	}

	return true;
}

class OggSink : public PcmSink
{
public:
	OggSink(OggWriter& writer, const std::string& target)
		: m_writer(writer), m_target(target)
	{
	}

	bool Begin(int channels, int rate, char* status, int statusSize) override
	{
		return m_writer.Begin(m_target, channels, rate, status, statusSize);
	}

	void Write(const short* interleaved, int frames) override
	{
		m_writer.Write(interleaved, frames);
	}

private:
	OggWriter& m_writer;
	std::string m_target;
};

class BufferSink : public PcmSink
{
public:
	explicit BufferSink(AudioFile::Pcm& out)
		: m_out(out)
	{
	}

	bool Begin(int channels, int rate, char*, int) override
	{
		m_out.channels = channels;
		m_out.rate = rate;
		return true;
	}

	void Write(const short* interleaved, int frames) override
	{
		m_out.samples.insert(m_out.samples.end(), interleaved,
			interleaved + (static_cast<size_t>(frames) * m_out.channels));
	}

private:
	AudioFile::Pcm& m_out;
};

bool DecodeInto(const std::vector<uint8_t>& bytes, PcmSink& sink, char* status, int statusSize)
{
	const AudioFile::Format format =
		AudioFile::IdentifyBytes(bytes.data(), static_cast<int>(bytes.size()));

	if (format == AudioFile::Format_Mp3)
		return DecodeMp3(bytes, sink, status, statusSize);

	if (format == AudioFile::Format_OggVorbis)
		return OggReader::Decode(bytes.data(), bytes.size(), sink, status, statusSize);

	if (format == AudioFile::Format_Wav)
		return DecodeWav(bytes, sink, status, statusSize);

	strncpy_s(status, statusSize, AudioFile::WhyItCannotPlay(format), _TRUNCATE);
	return false;
}

}

AudioFile::Format AudioFile::IdentifyBytes(const uint8_t* data, int size)
{
	if (data == nullptr || size < 16)
		return Format_Unknown;

	const int read = size < kHeaderBytes ? size : kHeaderBytes;

	if (IsWav(data, read))
		return Format_Wav;

	if (Tag(data, read, 0, "OggS"))
		return Contains(data, read, "vorbis") ? Format_OggVorbis : Format_OggOther;

	if (IsMp3(data, read))
		return Format_Mp3;

	return Format_Unknown;
}

AudioFile::Format AudioFile::Identify(const std::string& path)
{
	uint8_t header[kHeaderBytes] = {};
	int read = 0;

	if (!ReadHeader(path, header, kHeaderBytes, read))
		return Format_Unknown;

	return IdentifyBytes(header, read);
}

bool AudioFile::DecodeBytes(const std::vector<uint8_t>& bytes, Pcm& out, char* status,
	int statusSize)
{
	out.samples.clear();
	out.channels = 0;
	out.rate = 0;

	if (bytes.empty())
	{
		strncpy_s(status, statusSize, "is empty", _TRUNCATE);
		return false;
	}

	BufferSink sink(out);

	if (!DecodeInto(bytes, sink, status, statusSize))
		return false;

	if (out.samples.empty())
	{
		strncpy_s(status, statusSize, "holds no audio", _TRUNCATE);
		return false;
	}

	return true;
}

bool AudioFile::Decode(const std::string& path, Pcm& out, char* status, int statusSize)
{
	const std::vector<uint8_t> bytes = ReadWhole(path);

	if (bytes.empty())
	{
		strncpy_s(status, statusSize, "could not be read", _TRUNCATE);
		return false;
	}

	return DecodeBytes(bytes, out, status, statusSize);
}

const char* AudioFile::FormatName(Format format)
{
	switch (format)
	{
	case Format_Wav:
		return "WAV";
	case Format_OggVorbis:
		return "OGG Vorbis";
	case Format_OggOther:
		return "OGG, but not Vorbis";
	case Format_Mp3:
		return "MP3";
	default:
		break;
	}

	return "not a sound file";
}

const char* AudioFile::WhyItCannotPlay(Format format)
{
	if (format == Format_OggOther)
	{
		return "an .ogg holding Opus or FLAC, which the game cannot decode - re-encode it as "
			"Vorbis or MP3";
	}

	return "not audio the mod can read - use MP3, OGG Vorbis or WAV";
}

bool AudioFile::PlaysAsIs(Format format)
{
	return format == Format_OggVorbis;
}

bool AudioFile::CanConvert(Format format)
{
	return format == Format_Mp3 || format == Format_Wav;
}

bool AudioFile::ConvertToOgg(const std::string& source, const std::string& target, char* status,
	int statusSize)
{
	const Format format = Identify(source);

	if (!CanConvert(format))
	{
		strncpy_s(status, statusSize, WhyItCannotPlay(format), _TRUNCATE);
		return false;
	}

	const std::vector<uint8_t> bytes = ReadWhole(source);

	if (bytes.empty())
	{
		strncpy_s(status, statusSize, "could not be read", _TRUNCATE);
		return false;
	}

	OggWriter writer;
	OggSink sink(writer, target);

	if (!DecodeInto(bytes, sink, status, statusSize))
	{
		writer.Abort();
		DeleteFileA(target.c_str());
		return false;
	}

	writer.Finish();

	sprintf_s(status, statusSize, "converted from %s", FormatName(format));
	return true;
}
