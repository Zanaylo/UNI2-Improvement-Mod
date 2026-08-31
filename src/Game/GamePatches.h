#pragma once

#include "Game/PatchLibrary.h"

#include <string>

#include <Windows.h>

namespace GamePatches
{
	using Coverage = PatchLibrary::Coverage;
	using Patch = PatchLibrary::Patch;

	enum Answer
	{
		Answer_NotOurs,
		Answer_Missing,
		Answer_Found
	};

	void Load();

	int Count();
	const Patch* Get(int index);

	int ActiveIndex();
	int ChosenIndex();
	int BootIndex();

	int ReplayWanted();
	bool TablesAgreeWith(int index);
	void ApplyForReset(int index);

	void Choose(int index);

	void ApplyRemembered();
	void Update();

	int ForDate(const SYSTEMTIME& when);
	void OnReplayStarting(const SYSTEMTIME& when);

	bool IsAuto();
	void SetAuto(bool enabled);

	bool RebuildsTables();
	void SetRebuildsTables(bool enabled);
	bool CanRebuildTables();
	const char* RebuildStatus();

	const char* WhyActive();

	bool UnloadedForOnline();

	bool TakeAnnouncement(std::string& out);

	Answer Resolve(const char* path, std::string& out);

	int FilesRead();
	int FilesMissing();

	bool Import(const std::string& folder, const std::string& name, char* status, int statusSize);

	void SetDate(int index, const SYSTEMTIME& released);
	void Describe(int index, const std::string& note, const SYSTEMTIME& released);
	bool Forget(int index);

	std::string Root();
	const char* StatusText();
	bool IsSupported();
}
