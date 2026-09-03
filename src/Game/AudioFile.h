#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AudioFile
{
	enum Format
	{
		Format_Unknown,
		Format_Wav,
		Format_OggVorbis,
		Format_OggOther,
		Format_Mp3,
	};

	struct Pcm
	{
		std::vector<short> samples;
		int channels;
		int rate;
	};

	Format Identify(const std::string& path);
	Format IdentifyBytes(const uint8_t* data, int size);

	bool Decode(const std::string& path, Pcm& out, char* status, int statusSize);
	bool DecodeBytes(const std::vector<uint8_t>& bytes, Pcm& out, char* status, int statusSize);

	const char* FormatName(Format format);
	const char* WhyItCannotPlay(Format format);

	bool PlaysAsIs(Format format);
	bool CanConvert(Format format);

	bool ConvertToOgg(const std::string& source, const std::string& target, char* status,
		int statusSize);
}
