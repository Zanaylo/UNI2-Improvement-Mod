#pragma once

namespace ScriptReload
{
	bool IsSupported();

	bool Run();

	int Count();
	const char* StatusText();
}
