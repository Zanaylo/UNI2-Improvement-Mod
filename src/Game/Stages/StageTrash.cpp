#include "Game/Stages/StageTrash.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

volatile long g_running = 0;
volatile long g_again = 0;
volatile long g_serial = 0;

SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<std::string> g_inPlace;

std::string Root()
{
	return GetModRootPath("Trash");
}

std::string Leaf(const std::string& folder)
{
	const size_t slash = folder.find_last_of("\\/");

	return slash == std::string::npos ? folder : folder.substr(slash + 1);
}

bool IsDots(const char* name)
{
	return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

void EmptyTrash()
{
	WIN32_FIND_DATAA found = {};
	const std::string root = Root();
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || IsDots(found.cFileName))
			continue;

		StageTrash::DeleteNow(root + "\\" + found.cFileName);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

void EmptyInPlace()
{
	AcquireSRWLockShared(&g_lock);
	const std::vector<std::string> folders = g_inPlace;
	ReleaseSRWLockShared(&g_lock);

	for (const std::string& folder : folders)
	{
		StageTrash::DeleteNow(folder);

		AcquireSRWLockExclusive(&g_lock);
		g_inPlace.erase(std::remove(g_inPlace.begin(), g_inPlace.end(), folder), g_inPlace.end());
		ReleaseSRWLockExclusive(&g_lock);
	}
}

DWORD WINAPI Sweeper(void*)
{
	for (;;)
	{
		InterlockedExchange(&g_again, 0);

		const ULONGLONG began = GetTickCount64();

		EmptyInPlace();
		EmptyTrash();

		LOG("StageTrash: swept in %llu ms", GetTickCount64() - began);

		InterlockedExchange(&g_running, 0);

		if (InterlockedCompareExchange(&g_again, 0, 0) == 0
			|| InterlockedCompareExchange(&g_running, 1, 0) != 0)
		{
			return 0;
		}
	}
}

void Wake()
{
	InterlockedExchange(&g_again, 1);

	if (InterlockedCompareExchange(&g_running, 1, 0) != 0)
		return;

	const HANDLE thread = CreateThread(nullptr, 0, &Sweeper, nullptr, CREATE_SUSPENDED, nullptr);

	if (thread == nullptr)
	{
		InterlockedExchange(&g_running, 0);
		LOG("StageTrash: the sweeper could not be started, the trash is emptied on the next launch");
		return;
	}

	SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
	ResumeThread(thread);
	CloseHandle(thread);
}

}

void StageTrash::Discard(const std::string& folder)
{
	if (GetFileAttributesA(folder.c_str()) == INVALID_FILE_ATTRIBUTES)
		return;

	const std::string root = Root();
	CreateDirectoryA(root.c_str(), nullptr);

	char suffix[32] = {};
	sprintf_s(suffix, ".%llu.%ld", GetTickCount64(), InterlockedIncrement(&g_serial));

	const std::string parked = root + "\\" + Leaf(folder) + suffix;

	if (!MoveFileExA(folder.c_str(), parked.c_str(), 0))
	{
		LOG("StageTrash: %s could not be moved aside (error %lu), it is deleted where it is",
			folder.c_str(), GetLastError());

		AcquireSRWLockExclusive(&g_lock);
		g_inPlace.push_back(folder);
		ReleaseSRWLockExclusive(&g_lock);
	}

	Wake();
}

void StageTrash::DeleteNow(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (IsDots(found.cFileName))
				continue;

			const std::string path = folder + "\\" + found.cFileName;

			if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				DeleteNow(path);
			else
				DeleteFileA(path.c_str());
		}
		while (FindNextFileA(search, &found) != 0);

		FindClose(search);
	}

	RemoveDirectoryA(folder.c_str());
}

bool StageTrash::Pending(const std::string& folder)
{
	AcquireSRWLockShared(&g_lock);

	const bool pending = std::any_of(g_inPlace.begin(), g_inPlace.end(),
		[&folder](const std::string& queued) { return _stricmp(queued.c_str(), folder.c_str()) == 0; });

	ReleaseSRWLockShared(&g_lock);
	return pending;
}

void StageTrash::Sweep()
{
	if (GetFileAttributesA(Root().c_str()) == INVALID_FILE_ATTRIBUTES)
		return;

	Wake();
}
