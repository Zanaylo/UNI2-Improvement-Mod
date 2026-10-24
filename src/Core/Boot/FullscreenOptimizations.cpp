#include "Core/Boot/FullscreenOptimizations.h"

#include "Core/Boot/CompatLayerText.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"

#include <Windows.h>

#include <string>

namespace {

constexpr wchar_t kLayersKey[] = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";
constexpr wchar_t kDisablingLayer[] = L"DISABLEDXMAXIMIZEDWINDOWEDMODE";
constexpr wchar_t kLayerVariable[] = L"__COMPAT_LAYER";
constexpr DWORD kLongestPath = 32768;

struct LayersEntry
{
	std::wstring name;
	std::wstring layers;
};

std::wstring ExecutablePath()
{
	std::wstring path(kLongestPath, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, path.data(), kLongestPath);
	path.resize(length);
	return path;
}

bool FindEntry(HKEY key, const std::wstring& executable, LayersEntry& out)
{
	std::wstring name(kLongestPath, L'\0');
	std::wstring data(kLongestPath, L'\0');

	for (DWORD index = 0;; ++index)
	{
		DWORD nameLength = kLongestPath;
		DWORD dataBytes = kLongestPath * sizeof(wchar_t);
		DWORD type = 0;

		const LSTATUS status = RegEnumValueW(key, index, name.data(), &nameLength, nullptr, &type,
			reinterpret_cast<BYTE*>(data.data()), &dataBytes);

		if (status == ERROR_NO_MORE_ITEMS)
			return false;

		if (status != ERROR_SUCCESS || type != REG_SZ)
			continue;

		if (CompareStringOrdinal(name.c_str(), static_cast<int>(nameLength), executable.c_str(),
			static_cast<int>(executable.size()), TRUE) != CSTR_EQUAL)
		{
			continue;
		}

		out.name.assign(name.c_str(), nameLength);
		out.layers.assign(data.c_str());
		return true;
	}
}

bool Rewrite(HKEY key, const LayersEntry& entry, const std::wstring& layers)
{
	if (layers.empty())
		return RegDeleteValueW(key, entry.name.c_str()) == ERROR_SUCCESS;

	const DWORD bytes = static_cast<DWORD>((layers.size() + 1) * sizeof(wchar_t));
	return RegSetValueExW(key, entry.name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(layers.c_str()),
		bytes) == ERROR_SUCCESS;
}

void RestoreForUser(const std::wstring& executable)
{
	HKEY key = nullptr;

	if (RegOpenKeyExW(HKEY_CURRENT_USER, kLayersKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS)
		return;

	LayersEntry entry;

	if (FindEntry(key, executable, entry) && CompatLayerText::Contains(entry.layers, kDisablingLayer))
	{
		const std::wstring kept = CompatLayerText::Without(entry.layers, kDisablingLayer);
		const bool rewritten = Rewrite(key, entry, kept);

		LOG("FullscreenOptimizations: Windows had them turned off for this game ('%ls'); %s, from the next start "
			"exclusive fullscreen runs at full speed", entry.layers.c_str(),
			rewritten ? "turned back on" : "could not turn them back on");
	}

	RegCloseKey(key);
}

void ReportForMachine(const std::wstring& executable)
{
	HKEY key = nullptr;

	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kLayersKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
		return;

	LayersEntry entry;

	if (FindEntry(key, executable, entry) && CompatLayerText::Contains(entry.layers, kDisablingLayer))
	{
		LOG("FullscreenOptimizations: turned off for every user of this PC ('%ls'); untick 'Disable fullscreen "
			"optimizations' for all users in uni2.exe's properties", entry.layers.c_str());
	}

	RegCloseKey(key);
}

void ReportThisRun()
{
	std::wstring layers(kLongestPath, L'\0');
	const DWORD length = GetEnvironmentVariableW(kLayerVariable, layers.data(), kLongestPath);
	layers.resize(length < kLongestPath ? length : 0);

	if (!CompatLayerText::Contains(layers, kDisablingLayer))
		return;

	LOG("FullscreenOptimizations: off for this run ('%ls'), exclusive fullscreen is held to every other refresh",
		layers.c_str());
}

}

void FullscreenOptimizations::Restore()
{
	ReportThisRun();

	if (!g_modVals.restoreFullscreenOptimizations)
		return;

	const std::wstring executable = ExecutablePath();

	if (executable.empty())
		return;

	RestoreForUser(executable);
	ReportForMachine(executable);
}
