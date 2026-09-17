#pragma once

namespace GgpoLogCapture
{
	void SetEnabled(bool enabled);
	bool IsEnabled();

	void ReportSecond();

	const char* StatusText();
}
