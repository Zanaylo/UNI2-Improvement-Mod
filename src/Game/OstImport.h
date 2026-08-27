#pragma once

namespace OstImport
{
	enum Source
	{
		Source_None,
		Source_UNI,
		Source_MBTL,
		Source_MBAA,
	};

	Source Detect(const char* folder);
	const char* SourceName(Source source);
	bool IsSupported(Source source);

	bool Begin(const char* folder);

	void Update();

	bool IsBusy();
	int Progress();

	const char* StatusText();
}
