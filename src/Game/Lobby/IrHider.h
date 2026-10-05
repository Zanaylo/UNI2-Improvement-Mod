#pragma once

namespace IrHider
{
	bool Install();
	bool IsAvailable();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	long NumbersHidden();
	const char* StatusText();
}
