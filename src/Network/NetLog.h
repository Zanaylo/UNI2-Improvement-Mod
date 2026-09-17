#pragma once

#include <cstdarg>

namespace NetLog
{
	void Initialize();
	void Shutdown();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	void Write(const char* format, ...);
	void WriteV(const char* prefix, const char* format, va_list args);

	bool IsOverBudget();

	unsigned Dropped();
	const char* Path();
}
