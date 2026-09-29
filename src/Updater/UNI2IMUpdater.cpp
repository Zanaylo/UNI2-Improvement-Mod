#include "Core/info.h"
#include "Updater/Install.h"
#include "Updater/Standalone.h"
#include "Updater/UpdaterLog.h"
#include "Updater/UpdaterPaths.h"

#include <Windows.h>
#include <shellapi.h>

#include <string>

namespace {

std::wstring HandoffFromCommandLine()
{
	int count = 0;
	LPWSTR* const parts = CommandLineToArgvW(GetCommandLineW(), &count);

	std::wstring path;

	for (int i = 1; parts != nullptr && i + 1 < count; ++i)
	{
		if (std::wstring(parts[i]) == L"--handoff")
			path = parts[i + 1];
	}

	if (parts != nullptr)
		LocalFree(parts);

	return path;
}

int RunHandoff(const std::wstring& path)
{
	Handoff handoff;

	if (!Install::Load(path, handoff))
		return 2;

	UpdaterLog::Open(handoff.logPath);
	UpdaterLog::Write("updater started for %s", UpdaterPaths::Utf8(handoff.tag).c_str());

	if (!Install::Apply(handoff))
	{
		MessageBoxW(nullptr, L"The UNI2 Improvement Mod update could not be applied. Your "
			L"previous files are still in place. See UNI2-IM\\Updater\\logs\\updater.log for "
			L"details.", L"" UNI2_IM_NAME, MB_ICONERROR | MB_OK);

		return 1;
	}

	Install::Relaunch(handoff);
	return 0;
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	const std::wstring path = HandoffFromCommandLine();

	if (path.empty())
		return Standalone::Run();

	return RunHandoff(path);
}
