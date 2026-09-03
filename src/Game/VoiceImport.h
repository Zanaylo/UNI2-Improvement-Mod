#pragma once

#include <string>

namespace VoiceImport
{
	enum Source
	{
		Source_None,
		Source_UniLoose,
		Source_UniArchive,
	};

	Source Detect(const char* folder);
	const char* SourceName(Source source);
	bool IsSupported(Source source);

	bool Begin(const char* folder, int chara);

	void Update();

	bool IsBusy();
	int Progress();

	const char* StatusText();
	const char* PackId();
}
