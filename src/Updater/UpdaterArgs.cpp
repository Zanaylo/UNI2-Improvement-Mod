#include "Updater/UpdaterArgs.h"

#include "Core/info.h"

#include <shellapi.h>

#include <cwchar>

namespace {

constexpr const wchar_t* kHandoffFlag = L"--handoff";
constexpr const wchar_t* kUpdateFlag = L"" UNI2_IM_UPDATER_UPDATE_FLAG;
constexpr const wchar_t* kParentFlag = L"" UNI2_IM_UPDATER_PARENT_FLAG;
constexpr int kDecimal = 10;

DWORD ProcessId(const std::wstring& text)
{
	wchar_t* end = nullptr;
	const unsigned long value = std::wcstoul(text.c_str(), &end, kDecimal);

	if (end == text.c_str() || *end != L'\0')
		return 0;

	return static_cast<DWORD>(value);
}

}

UpdaterArgs::Request UpdaterArgs::Parse(const std::vector<std::wstring>& arguments)
{
	Request request;

	for (size_t i = 0; i < arguments.size(); ++i)
	{
		const std::wstring& flag = arguments[i];

		if (flag == kUpdateFlag)
			request.mode = Mode::GameUpdate;

		if (i + 1 >= arguments.size())
			continue;

		if (flag == kHandoffFlag)
			request.handoffPath = arguments[i + 1];

		if (flag == kParentFlag)
			request.parentPid = ProcessId(arguments[i + 1]);
	}

	if (request.mode == Mode::Standalone && !request.handoffPath.empty())
		request.mode = Mode::Handoff;

	return request;
}

UpdaterArgs::Request UpdaterArgs::FromCommandLine()
{
	int count = 0;
	LPWSTR* const parts = CommandLineToArgvW(GetCommandLineW(), &count);

	if (parts == nullptr)
		return Request();

	std::vector<std::wstring> arguments;

	for (int i = 1; i < count; ++i)
		arguments.emplace_back(parts[i]);

	LocalFree(parts);
	return Parse(arguments);
}
