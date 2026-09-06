#include "Game/BgGrade.h"

#include "Core/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgShaderText.h"
#include "Game/FbGameFolder.h"
#include "Game/GameOffsets.h"
#include "Game/ModFiles.h"
#include "Game/StageImport.h"
#include "Hooks/HookManager.h"

#include <Windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace {

constexpr const char* kSection = "StageColour";
constexpr const char* kStale = "Mods\\Shader\\sh_bgspeculer.txt";
constexpr int kBlackRegister = 220;
constexpr int kContrastRegister = 221;
constexpr int kSetPixelShaderConstantF = 109;

using CreateEffect_t = HRESULT(WINAPI*)(void*, const void*, UINT, const void*, void*, DWORD,
	void*, void**, void**);

using SetPixelShaderConstantF_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT,
	const float*, UINT);

CreateEffect_t oCreateEffect = nullptr;
SetPixelShaderConstantF_t oSetPixelShaderConstantF = nullptr;

char g_status[224] = "the game's own curve, unchanged";
volatile long g_altered = 0;
volatile long g_reached = 0;
volatile long g_dirty = 1;
volatile long g_effects = 0;
int g_stage = -1;

std::map<int, BgGrade::Grade> g_cache;

float g_live[8] = {
	BgGrade::kGameLift, BgGrade::kGameLift, BgGrade::kGameLift, 1.0f,
	BgGrade::kGameContrast, BgGrade::kGameContrast, BgGrade::kGameContrast, 1.0f,
};

std::string Key(int stage)
{
	char key[16] = {};
	sprintf_s(key, "Stage%d", stage);

	return key;
}

bool Rewrite(const char* source, size_t bytes, std::string& out)
{
	const std::string text(source, bytes);

	if (!BgShaderText::IsBackground(text))
		return false;

	BgShaderText::Result result = {};

	if (!BgShaderText::Rewrite(text, kBlackRegister, kContrastRegister, BgGrade::kGameLift, out,
		result))
	{
		const std::string assignment = BgShaderText::Assignment(text);

		LOG("BgGrade: the background shader reads '%s' and the mod did not know how to open it",
			assignment.empty() ? "(no out_color.rgb assignment)" : assignment.c_str());

		return false;
	}

	sprintf_s(g_status, "live, on the background shader (%d lift, %d contrast)",
		result.black, result.contrast);

	LOG("BgGrade: %s", g_status);
	return true;
}

HRESULT WINAPI HookedCreateEffect(void* device, const void* source, UINT bytes,
	const void* defines, void* include, DWORD flags, void* pool, void** effect, void** errors)
{
	const long seen = InterlockedIncrement(&g_effects);

	if (seen <= 16)
		LOG("BgGrade: effect %ld compiled, %u byte(s)", seen, bytes);

	std::string rewritten;

	if (source != nullptr && bytes != 0
		&& Rewrite(static_cast<const char*>(source), bytes, rewritten))
	{
		InterlockedExchange(&g_reached, 1);

		return oCreateEffect(device, rewritten.data(), static_cast<UINT>(rewritten.size()),
			defines, include, flags, pool, effect, errors);
	}

	return oCreateEffect(device, source, bytes, defines, include, flags, pool, effect, errors);
}

HRESULT STDMETHODCALLTYPE HookedSetPixelShaderConstantF(IDirect3DDevice9* device, UINT start,
	const float* data, UINT count)
{
	const HRESULT result = oSetPixelShaderConstantF(device, start, data, count);

	if (InterlockedCompareExchange(&g_altered, 0, 0) == 0)
		return result;

	const UINT stop = start + count;

	if (start <= static_cast<UINT>(kContrastRegister)
		&& stop > static_cast<UINT>(kBlackRegister))
	{
		oSetPixelShaderConstantF(device, kBlackRegister, g_live, 2);
	}

	return result;
}

}

bool BgGrade::Initialize()
{
	const std::string stale = GetModRootPath(kStale);

	if (GetFileAttributesA(stale.c_str()) != INVALID_FILE_ATTRIBUTES && DeleteFileA(stale.c_str()))
	{
		ModFiles::Rescan();

		LOG("BgGrade: removed %s, which an older build wrote and which hid the live curve",
			stale.c_str());
	}

	if (!HookManager::CreateApiHook("d3dx9_42.dll", "D3DXCreateEffect", &HookedCreateEffect,
		reinterpret_cast<void**>(&oCreateEffect)))
	{
		strncpy_s(g_status, "the effect compiler could not be hooked, so the curve is the "
			"game's own", _TRUNCATE);
		LOG("BgGrade: %s", g_status);
		return false;
	}

	return true;
}

void BgGrade::Attach(IDirect3DDevice9* device)
{
	if (device == nullptr || oSetPixelShaderConstantF != nullptr)
		return;

	HookManager::CreateVTableHook(device, kSetPixelShaderConstantF,
		&HookedSetPixelShaderConstantF, reinterpret_cast<void**>(&oSetPixelShaderConstantF),
		"SetPixelShaderConstantF");
}

BgGrade::Grade BgGrade::DefaultOf(int stage)
{
	Grade grade = { kGameLift, kGameContrast };

	for (int i = 0; i < StageImport::PortCount(); ++i)
	{
		const StageImport::Port* const port = StageImport::PortAt(i);

		if (port == nullptr || port->number != stage)
			continue;

		if (port->game == FbGameFolder::Name(FbGameFolder::Game_DFCI))
		{
			grade.lift = kDfciLift;
			grade.contrast = kDfciContrast;
		}

		break;
	}

	return grade;
}

BgGrade::Grade BgGrade::Of(int stage)
{
	const std::map<int, Grade>::const_iterator known = g_cache.find(stage);

	if (known != g_cache.end())
		return known->second;

	Grade grade = DefaultOf(stage);

	char stored[64] = {};

	GetPrivateProfileStringA(kSection, Key(stage).c_str(), "", stored, sizeof(stored),
		Settings::GetIniPath().c_str());

	float lift = grade.lift;
	float contrast = grade.contrast;

	if (stored[0] != 0 && sscanf_s(stored, "%f,%f", &lift, &contrast) == 2)
	{
		grade.lift = lift;
		grade.contrast = contrast;
	}

	g_cache[stage] = grade;
	return grade;
}

void BgGrade::Set(int stage, const Grade& grade)
{
	char value[64] = {};
	sprintf_s(value, "%.3f,%.3f", grade.lift, grade.contrast);

	Settings::SaveString(kSection, Key(stage).c_str(), value);

	g_cache[stage] = grade;
	InterlockedExchange(&g_dirty, 1);
}

void BgGrade::Forget(int stage)
{
	Settings::SaveString(kSection, Key(stage).c_str(), "");

	g_cache.erase(stage);
	InterlockedExchange(&g_dirty, 1);
}

void BgGrade::Update()
{
	if (InterlockedCompareExchange(&g_reached, 0, 0) == 0)
		return;

	const uintptr_t address = RvaToAddress(GameOffsets::kBgPendingNumber);

	if (!IsAddressInGameModule(address))
		return;

	const int stage = *reinterpret_cast<const int*>(address);
	const long dirty = InterlockedExchange(&g_dirty, 0);

	if (stage == g_stage && dirty == 0)
		return;

	if (stage != g_stage)
		LOG("BgGrade: the stage the game is holding changed to %d", stage);

	g_stage = stage;

	const Grade grade = Of(stage);

	for (int i = 0; i < 3; ++i)
	{
		g_live[i] = grade.lift;
		g_live[4 + i] = grade.contrast;
	}

	const bool own = grade.lift == kGameLift && grade.contrast == kGameContrast;

	InterlockedExchange(&g_altered, own ? 0 : 1);
}

bool BgGrade::Reached()
{
	return InterlockedCompareExchange(&g_reached, 0, 0) != 0;
}

const char* BgGrade::StatusText()
{
	return g_status;
}
