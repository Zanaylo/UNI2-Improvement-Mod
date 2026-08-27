#pragma once

namespace CrashContext
{
	using Writer = void (*)();

	void Register(const char* name, Writer writer);
	void WriteAll();
}
