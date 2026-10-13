#include "Core/info.h"
#include "Updater/Install.h"
#include "Updater/Standalone.h"
#include "Updater/UpdaterArgs.h"
#include "Updater/UpdaterLog.h"
#include "Updater/UpdaterPaths.h"

#include <Windows.h>

#include <string>

namespace {

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
	const UpdaterArgs::Request request = UpdaterArgs::FromCommandLine();

	if (request.mode == UpdaterArgs::Mode::Handoff)
		return RunHandoff(request.handoffPath);

	if (request.mode == UpdaterArgs::Mode::GameUpdate)
		return Standalone::RunForGame(request.parentPid);

	return Standalone::Run();
}
