#pragma once

namespace ScriptConsole
{
	bool SetOpen(bool open);
	bool IsOpen();

	long Lines();
	const char* StatusText();
}
