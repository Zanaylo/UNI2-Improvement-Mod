#pragma once

#include <string>

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

	Format Identify(const std::string& path);

	const char* FormatName(Format format);
	const char* WhyItCannotPlay(Format format);

	bool PlaysAsIs(Format format);
	bool CanConvert(Format format);

	bool ConvertToOgg(const std::string& source, const std::string& target, char* status,
		int statusSize);
}
