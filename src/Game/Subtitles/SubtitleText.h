#pragma once

namespace SubtitleText
{
	bool Install();
	bool IsAvailable();

	bool AnsweredRecently(int chara, int index);
	bool Draw(int slot, int chara, int index, int frames);

	int Answers();
	int Asked();
	const char* StatusText();
}
