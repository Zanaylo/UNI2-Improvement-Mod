#include "Hooks/InputProbe.h"

#include "Core/logger.h"
#include "Hooks/HookManager.h"

#include <MinHook.h>

#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace {

constexpr int kCreateDeviceIndex = 3;
constexpr int kGetDeviceStateIndex = 9;
constexpr int kGetDeviceDataIndex = 10;

using CreateDevice_t = HRESULT(STDMETHODCALLTYPE*)(IDirectInput8A*, REFGUID, LPDIRECTINPUTDEVICE8A*,
	LPUNKNOWN);
using GetDeviceState_t = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8A*, DWORD, LPVOID);
using GetDeviceData_t = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8A*, DWORD,
	LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

CreateDevice_t oCreateDevice = nullptr;
GetDeviceState_t oGetDeviceState = nullptr;
GetDeviceData_t oGetDeviceData = nullptr;

void* g_pending = nullptr;
bool g_active = false;
bool g_deviceHooked = false;

int g_devicesCreated = 0;
long g_stateCalls = 0;
long g_dataCalls = 0;
long g_stateBytes = 0;
DWORD g_lastReport = 0;

long g_asyncCalls = 0;
long g_rawCalls = 0;
long g_keyboardStateCalls = 0;
long g_keyStateCalls = 0;
long g_keyMessages = 0;
long g_rawMessages = 0;

volatile DWORD g_samplerThread = 0;

constexpr DWORD kJoystick2StateSize = 0x110;
constexpr int kMaxPads = 8;

IDirectInputDevice8A* g_pads[kMaxPads] = {};
volatile LONG g_padCount = 0;

void RememberPad(IDirectInputDevice8A* device)
{
	const LONG count = InterlockedCompareExchange(&g_padCount, 0, 0);

	for (LONG i = 0; i < count; ++i)
	{
		if (g_pads[i] == device)
			return;
	}

	if (count >= kMaxPads)
		return;

	g_pads[count] = device;
	InterlockedExchange(&g_padCount, count + 1);

	LOG("input probe: pad device %p kept for sampling", static_cast<void*>(device));
}

bool HookVTableEntry(void** vtable, int index, void* detour, void** original, const char* label)
{
	return HookManager::CreateAndEnableHook(vtable[index], detour, original, label);
}

const GUID kSysKeyboard = { 0x6f1d2b61, 0xd5a0, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
const GUID kSysMouse    = { 0x6f1d2b60, 0xd5a0, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
const GUID kJoystick    = { 0x6f1d2b70, 0xd5a0, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };

const char* DeviceName(REFGUID guid)
{
	if (IsEqualGUID(guid, kSysKeyboard))
		return "SysKeyboard";
	if (IsEqualGUID(guid, kSysMouse))
		return "SysMouse";
	if (IsEqualGUID(guid, kJoystick))
		return "Joystick";

	return "pad or other (instance GUID)";
}

HRESULT STDMETHODCALLTYPE HookedGetDeviceState(IDirectInputDevice8A* device, DWORD size, LPVOID data)
{
	if (GetCurrentThreadId() != g_samplerThread)
	{
		++g_stateCalls;
		g_stateBytes = static_cast<long>(size);

		if (size == kJoystick2StateSize)
			RememberPad(device);
	}

	return oGetDeviceState(device, size, data);
}

HRESULT STDMETHODCALLTYPE HookedGetDeviceData(IDirectInputDevice8A* device, DWORD objectSize,
	LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags)
{
	++g_dataCalls;
	return oGetDeviceData(device, objectSize, data, count, flags);
}

HRESULT STDMETHODCALLTYPE HookedCreateDevice(IDirectInput8A* self, REFGUID guid,
	LPDIRECTINPUTDEVICE8A* device, LPUNKNOWN outer)
{
	const HRESULT result = oCreateDevice(self, guid, device, outer);

	if (FAILED(result) || device == nullptr || *device == nullptr)
		return result;

	++g_devicesCreated;
	LOG("input probe: device %d created, %s {%08lX-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}",
		g_devicesCreated, DeviceName(guid),
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	if (!g_deviceHooked)
	{
		void** vtable = *reinterpret_cast<void***>(*device);

		const bool state = HookVTableEntry(vtable, kGetDeviceStateIndex, &HookedGetDeviceState,
			reinterpret_cast<void**>(&oGetDeviceState), "IDirectInputDevice8::GetDeviceState");
		const bool data = HookVTableEntry(vtable, kGetDeviceDataIndex, &HookedGetDeviceData,
			reinterpret_cast<void**>(&oGetDeviceData), "IDirectInputDevice8::GetDeviceData");

		g_deviceHooked = state || data;
	}

	return result;
}

}

void InputProbe::OnInterfaceCreated(void* directInput)
{
	if (directInput == nullptr || g_pending != nullptr || g_active)
		return;

	g_pending = directInput;
	LOG("input probe: DirectInput8 interface seen, hooking from the first frame");
}

void InputProbe::OnFrame()
{
	if (!g_active && g_pending != nullptr)
	{
		void** vtable = *reinterpret_cast<void***>(g_pending);

		g_active = HookVTableEntry(vtable, kCreateDeviceIndex, &HookedCreateDevice,
			reinterpret_cast<void**>(&oCreateDevice), "IDirectInput8::CreateDevice");

		LOG("input probe: %s", g_active ? "watching CreateDevice" : "could not hook CreateDevice");

		g_pending = nullptr;
	}

	if (!g_active)
		return;

	const DWORD now = GetTickCount();
	if (g_lastReport != 0 && now - g_lastReport < 1000)
		return;

	g_lastReport = now;

	LOG("input probe: dinput %d device(s) state x%ld data x%ld | user32 keyboardState x%ld keyState x%ld "
		"asyncKeyState x%ld rawInputData x%ld | messages key x%ld raw x%ld",
		g_devicesCreated, g_stateCalls, g_dataCalls,
		g_keyboardStateCalls, g_keyStateCalls, g_asyncCalls, g_rawCalls,
		g_keyMessages, g_rawMessages);

	g_stateCalls = 0;
	g_dataCalls = 0;
	g_keyboardStateCalls = 0;
	g_keyStateCalls = 0;
	g_asyncCalls = 0;
	g_rawCalls = 0;
	g_keyMessages = 0;
	g_rawMessages = 0;
}

bool InputProbe::IsActive()
{
	return g_active;
}

namespace {

using GetAsyncKeyState_t = SHORT(WINAPI*)(int);
using GetRawInputData_t = UINT(WINAPI*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
using RegisterRawInputDevices_t = BOOL(WINAPI*)(PCRAWINPUTDEVICE, UINT, UINT);

GetAsyncKeyState_t oGetAsyncKeyState = nullptr;
GetRawInputData_t oGetRawInputData = nullptr;
RegisterRawInputDevices_t oRegisterRawInputDevices = nullptr;

SHORT WINAPI HookedGetAsyncKeyState(int key)
{
	if (GetCurrentThreadId() != g_samplerThread)
		++g_asyncCalls;

	return oGetAsyncKeyState(key);
}

UINT WINAPI HookedGetRawInputData(HRAWINPUT input, UINT command, LPVOID data, PUINT size,
	UINT headerSize)
{
	++g_rawCalls;
	return oGetRawInputData(input, command, data, size, headerSize);
}

BOOL WINAPI HookedRegisterRawInputDevices(PCRAWINPUTDEVICE devices, UINT count, UINT size)
{

	for (UINT i = 0; devices != nullptr && i < count; ++i)
	{
		LOG("input probe: raw input registered, usage page %u usage %u flags 0x%lx",
			devices[i].usUsagePage, devices[i].usUsage, devices[i].dwFlags);
	}

	return oRegisterRawInputDevices(devices, count, size);
}

}

bool InputProbe::InstallApiProbes()
{
	bool ok = true;

	ok &= HookManager::CreateApiHook("user32.dll", "GetAsyncKeyState", &HookedGetAsyncKeyState,
		reinterpret_cast<void**>(&oGetAsyncKeyState));
	ok &= HookManager::CreateApiHook("user32.dll", "GetRawInputData", &HookedGetRawInputData,
		reinterpret_cast<void**>(&oGetRawInputData));
	ok &= HookManager::CreateApiHook("user32.dll", "RegisterRawInputDevices",
		&HookedRegisterRawInputDevices, reinterpret_cast<void**>(&oRegisterRawInputDevices));

	if (!ok)
		LOG("input probe: some user32 probes failed");

	return ok;
}

void InputProbe::CountKeyboardState()
{
	++g_keyboardStateCalls;
}

void InputProbe::CountKeyState()
{
	++g_keyStateCalls;
}

void InputProbe::SetSamplerThread(unsigned long threadId)
{
	g_samplerThread = static_cast<DWORD>(threadId);
}

void InputProbe::CountWindowMessage(unsigned int message)
{
	if (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN ||
		message == WM_SYSKEYUP)
	{
		++g_keyMessages;
	}
	else if (message == WM_INPUT)
	{
		++g_rawMessages;
	}
}

int InputProbe::GetPadDeviceCount()
{
	return static_cast<int>(InterlockedCompareExchange(&g_padCount, 0, 0));
}

void* InputProbe::GetPadDevice(int index)
{
	if (index < 0 || index >= GetPadDeviceCount())
		return nullptr;

	return g_pads[index];
}
