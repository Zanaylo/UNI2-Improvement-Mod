#pragma once

namespace UiText
{
	void Help(const char* text);

	void Muted(const char* format, ...);
	void Warn(const char* format, ...);
	void Good(const char* format, ...);
}
