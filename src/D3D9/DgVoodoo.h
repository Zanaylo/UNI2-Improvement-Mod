#pragma once

#include <Windows.h>

#include <string>

namespace DgVoodoo
{
	enum Choice
	{
		Choice_Resolution,
		Choice_Scaling,
		Choice_Filtering,
		Choice_Antialiasing,
		Choice_Output,
		Choice_COUNT
	};

	enum Flag
	{
		Flag_FakeFullscreen,
		Flag_VSync,
		Flag_KeepAspect,
		Flag_CaptureMouse,
		Flag_Watermark,
		Flag_COUNT
	};

	HMODULE Load();

	bool IsInstalled();
	bool IsRunning();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	int ChoiceCount(Choice choice);
	const char* ChoiceName(Choice choice);
	const char* ChoiceLabel(Choice choice, int option);
	int Chosen(Choice choice);
	void Choose(Choice choice, int option);

	const char* FlagName(Flag flag);
	bool IsSet(Flag flag);
	void Set(Flag flag, bool set);

	int FpsLimit();
	void SetFpsLimit(int fps);

	void Save();

	bool InstallFrom(const std::string& folder, char* status, int statusSize);

	std::string Folder();
	const char* StatusText();
}
