#include "Game/BgGrade.h"

#include "Core/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/DeviceHooks.h"
#include "Game/BgShaderText.h"
#include "Game/BgVertexProbe.h"
#include "Game/FbGameFolder.h"
#include "Game/GameOffsets.h"
#include "Game/ModFiles.h"
#include "Game/StageLibrary.h"
#include "Game/StageSettingKey.h"
#include "Hooks/HookManager.h"
#include "Training/FrameStepper.h"

#include <Windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr const char* kSection = "StageColour";
constexpr const char* kStale = "Mods\\Shader\\sh_bgspeculer.txt";
constexpr int kLampRegister = 218;
constexpr int kLampSlots = 8;
constexpr int kLampRamps = 512;
constexpr size_t kNoteLine = 16384;
constexpr float kLampFull = 1000.0f;
constexpr int kBlackRegister = 220;
constexpr int kContrastRegister = 221;
constexpr int kFlowRegister = 222;
constexpr int kFlowSlots = 8;
constexpr int kFirstRegister = kLampRegister;
constexpr int kRegisters = kFlowRegister - kFirstRegister + kFlowSlots / 4;
constexpr int kLampFirst = 4 * (kLampRegister - kFirstRegister);
constexpr int kLiftFirst = 4 * (kBlackRegister - kFirstRegister);
constexpr int kContrastFirst = 4 * (kContrastRegister - kFirstRegister);
constexpr int kFlowFirst = 4 * (kFlowRegister - kFirstRegister);
constexpr int kSetPixelShaderConstantF = 109;
constexpr int kSetRenderState = 57;
constexpr int kDestBlend = 20;
constexpr int kBlendOne = 2;
constexpr float kSameGrade = 0.0005f;
constexpr long kReported = 8;

using CreateEffect_t = HRESULT(WINAPI*)(void*, const void*, UINT, const void*, void*, DWORD,
	void*, void**, void**);

using SetRenderState_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,
	D3DRENDERSTATETYPE, DWORD);
using SetPixelShaderConstantF_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT,
	const float*, UINT);

CreateEffect_t oCreateEffect = nullptr;
SetPixelShaderConstantF_t oSetPixelShaderConstantF = nullptr;
SetRenderState_t oSetRenderState = nullptr;

bool From(const std::string& game, FbGameFolder::Game source)
{
	return game == FbGameFolder::Name(source);
}

bool Arcsys(const std::string& game)
{
	return From(game, FbGameFolder::Game_BBTAG) || From(game, FbGameFolder::Game_BBCF);
}

bool Flows(int stage)
{
	StageLibrary::Entry entry = {};

	return StageLibrary::Of(stage, entry) && Arcsys(entry.game);
}

char g_status[224] = "the game's own curve, unchanged";
volatile long g_altered = 0;
volatile long g_flowing = 0;
volatile long g_lighting = 0;
volatile long g_fading = 0;
volatile long g_touched = 0;
volatile long g_reached = 0;
volatile long g_dirty = 1;
volatile long g_effects = 0;
int g_stage = -1;

std::map<int, BgGrade::Grade> g_cache;
std::map<int, float> g_glowCache;
std::map<int, bool> g_offCache;

float g_live[4 * kRegisters] = {
	1.0f, 1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f, 1.0f,
	BgGrade::kGameLift, BgGrade::kGameLift, BgGrade::kGameLift, 0.0f,
	BgGrade::kGameContrast, BgGrade::kGameContrast, BgGrade::kGameContrast, 1.0f,
};

struct Lamp
{
	int loop;
	int ramps;
	int at[kLampRamps];
	int target[kLampRamps];
	int frames[kLampRamps];
};

float LampAt(const Lamp& lamp, int frame)
{
	if (lamp.loop < 1 || lamp.ramps < 1)
		return 1.0f;

	const int at = ((frame % lamp.loop) + lamp.loop) % lamp.loop;

	float held = static_cast<float>(lamp.target[lamp.ramps - 1]) / kLampFull;
	float value = held;

	for (int i = 0; i < lamp.ramps; ++i)
	{
		if (lamp.at[i] > at)
			break;

		const float target = static_cast<float>(lamp.target[i]) / kLampFull;
		const int step = at - lamp.at[i];
		const int span = lamp.frames[i] < 1 ? 1 : lamp.frames[i];

		value = step >= span ? target
			: held + (target - held) * (static_cast<float>(step) / span);
		held = target;
	}

	return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

float g_rate[kFlowSlots] = {};
float g_glow = 1.0f;
Lamp g_lamp[kLampSlots] = {};
int g_frame = 0;

bool Owning()
{
	return InterlockedCompareExchange(&g_altered, 0, 0) != 0
		|| InterlockedCompareExchange(&g_flowing, 0, 0) != 0
		|| InterlockedCompareExchange(&g_lighting, 0, 0) != 0
		|| InterlockedCompareExchange(&g_fading, 0, 0) != 0;
}

void Upload()
{
	IDirect3DDevice9* const device = DeviceHooks::GetDevice();

	if (device == nullptr || oSetPixelShaderConstantF == nullptr)
		return;

	oSetPixelShaderConstantF(device, kFirstRegister, g_live, kRegisters);
}

void Assert()
{
	if (!Owning())
		return;

	Upload();
}

void Lamps()
{
	if (InterlockedCompareExchange(&g_lighting, 0, 0) == 0)
		return;

	++g_frame;

	for (int i = 0; i < kLampSlots; ++i)
		g_live[kLampFirst + i] = LampAt(g_lamp[i], g_frame);
}

void Flow()
{
	if (InterlockedCompareExchange(&g_flowing, 0, 0) == 0)
		return;

	for (int i = 0; i < kFlowSlots; ++i)
	{
		if (g_rate[i] == 0.0f)
			continue;

		float& offset = g_live[kFlowFirst + i];
		offset += g_rate[i];

		while (offset >= 1.0f)
			offset -= 1.0f;

		while (offset <= -1.0f)
			offset += 1.0f;
	}
}

struct Note
{
	float rate[kFlowSlots];
	Lamp lamp[kLampSlots];
	bool flowing;
	bool lighting;
	bool fading;
};


void ReadLamp(const char* line, Note& note)
{
	const char* const digits = strpbrk(line, "0123456789");

	if (digits == nullptr)
		return;

	const int slot = atoi(digits);
	const char* open = strchr(line, '[');

	if (slot < 0 || slot >= kLampSlots || open == nullptr)
		return;

	Lamp& lamp = note.lamp[slot];
	lamp.loop = atoi(open + 1);
	lamp.ramps = 0;

	open = strchr(open + 1, ',');

	while (open != nullptr && lamp.ramps < kLampRamps)
	{
		const int at = atoi(open + 1);
		open = strchr(open + 1, ',');

		if (open == nullptr)
			break;

		const int target = atoi(open + 1);
		open = strchr(open + 1, ',');

		if (open == nullptr)
			break;

		lamp.at[lamp.ramps] = at;
		lamp.target[lamp.ramps] = target;
		lamp.frames[lamp.ramps] = atoi(open + 1);
		++lamp.ramps;

		open = strchr(open + 1, ',');
	}

	note.lighting = note.lighting || lamp.ramps > 0;
}

void ReadRates(const char* line, Note& note)
{
	const char* open = strchr(line, '[');

	if (open == nullptr)
		return;

	for (int i = 0; i < kFlowSlots && open != nullptr; ++i)
	{
		note.rate[i] = static_cast<float>(atof(open + 1));
		note.flowing = note.flowing || note.rate[i] != 0.0f;
		open = strchr(open + 1, ',');
	}
}

bool ReadNote(int stage, Note& note)
{
	memset(&note, 0, sizeof(note));

	const std::string folder = StageLibrary::FolderOf(stage);

	if (folder.empty())
		return false;

	FILE* handle = nullptr;

	if (fopen_s(&handle, (folder + "\\stage.txt").c_str(), "rb") != 0 || handle == nullptr)
		return false;

	std::vector<char> buffer(kNoteLine);

	while (fgets(buffer.data(), static_cast<int>(buffer.size()), handle) != nullptr)
	{
		const char* const line = buffer.data();

		if (strstr(line, "VertexAlpha") != nullptr)
		{
			const char* const equals = strchr(line, '=');

			note.fading = equals != nullptr && atoi(equals + 1) != 0;
			continue;
		}

		if (strstr(line, "Flow") != nullptr)
		{
			ReadRates(line, note);
			continue;
		}

		if (strstr(line, "Lamp") != nullptr)
			ReadLamp(line, note);
	}

	fclose(handle);

	return true;
}

std::string Key(int stage)
{
	char key[16] = {};
	sprintf_s(key, "Stage%d", stage);

	return StageSettingKey::For(kSection, stage, key, { "", "Glow", "Off" });
}

bool Same(float one, float other)
{
	const float apart = one - other;

	return apart > -kSameGrade && apart < kSameGrade;
}

bool Rewrite(const char* source, size_t bytes, std::string& out)
{
	const std::string text(source, bytes);

	if (!BgShaderText::IsBackground(text))
		return false;

	BgShaderText::Result result = {};

	if (!BgShaderText::Rewrite(text, kBlackRegister, kContrastRegister, kFlowRegister,
		kLampRegister, BgGrade::kGameLift, out, result))
	{
		const std::string assignment = BgShaderText::Assignment(text);

		LOG("BgGrade: the background shader reads '%s' and the mod did not know how to open it",
			assignment.empty() ? "(no out_color.rgb assignment)" : assignment.c_str());

		return false;
	}

	sprintf_s(g_status, "on, in the stage shader (%d lift, %d contrast, %d flow, "
		"%d fade)", result.black, result.contrast, result.flow, result.fade);

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
		const HRESULT graded = oCreateEffect(device, rewritten.data(),
			static_cast<UINT>(rewritten.size()), defines, include, flags, pool, effect, errors);

		if (SUCCEEDED(graded))
		{
			InterlockedExchange(&g_reached, 1);
			return graded;
		}

		strncpy_s(g_status, "the changed stage shader did not compile, so the game's own shader is "
			"used", _TRUNCATE);

		LOG("BgGrade: %s", g_status);
	}

	return oCreateEffect(device, source, bytes, defines, include, flags, pool, effect, errors);
}

HRESULT STDMETHODCALLTYPE HookedSetRenderState(IDirect3DDevice9* device,
	D3DRENDERSTATETYPE state, DWORD value)
{
	const HRESULT result = oSetRenderState(device, state, value);

	if (state != kDestBlend || oSetPixelShaderConstantF == nullptr || !Owning())
		return result;

	const float wanted = value == kBlendOne ? g_glow : 1.0f;

	if (g_live[kContrastFirst + 3] == wanted)
		return result;

	g_live[kContrastFirst + 3] = wanted;
	oSetPixelShaderConstantF(device, kContrastRegister, g_live + kContrastFirst, 1);

	return result;
}

HRESULT STDMETHODCALLTYPE HookedSetPixelShaderConstantF(IDirect3DDevice9* device, UINT start,
	const float* data, UINT count)
{
	const HRESULT result = oSetPixelShaderConstantF(device, start, data, count);

	if (!Owning())
		return result;

	const UINT stop = start + count;

	if (start < static_cast<UINT>(kFirstRegister + kRegisters)
		&& stop > static_cast<UINT>(kFirstRegister))
	{
		const long seen = InterlockedIncrement(&g_touched);

		if (seen <= kReported)
			LOG("BgGrade: the game wrote c%u..c%u, so the mod put its own four back",
				start, stop - 1);

		BgVertexProbe::Arm();
		oSetPixelShaderConstantF(device, kFirstRegister, g_live, kRegisters);
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

	const HMODULE compiler = GetModuleHandleA("d3dx9_42.dll");
	void* const entry = compiler == nullptr
		? nullptr : reinterpret_cast<void*>(GetProcAddress(compiler, "D3DXCreateEffect"));

	if (entry == nullptr || !HookManager::CreateAndEnableHook(entry, &HookedCreateEffect,
		reinterpret_cast<void**>(&oCreateEffect), "D3DXCreateEffect"))
	{
		strncpy_s(g_status, "the shader compiler could not be hooked, so the stage keeps the "
			"game's own colours", _TRUNCATE);
		LOG("BgGrade: %s", g_status);
		return false;
	}

	LOG("BgGrade: the effect compiler is hooked and armed");
	return true;
}

void BgGrade::Attach(IDirect3DDevice9* device)
{
	if (device == nullptr || oSetPixelShaderConstantF != nullptr)
		return;

	void** const vtable = *reinterpret_cast<void***>(device);

	HookManager::CreateAndEnableHook(vtable[kSetPixelShaderConstantF],
		&HookedSetPixelShaderConstantF, reinterpret_cast<void**>(&oSetPixelShaderConstantF),
		"SetPixelShaderConstantF");

	HookManager::CreateAndEnableHook(vtable[kSetRenderState], &HookedSetRenderState,
		reinterpret_cast<void**>(&oSetRenderState), "SetRenderState");
}

BgGrade::Grade BgGrade::DefaultOf(int stage)
{
	StageLibrary::Entry entry = {};

	if (!StageLibrary::Of(stage, entry))
		return Grade{ kGameLift, kGameContrast };

	if (From(entry.game, FbGameFolder::Game_DFCI))
		return Grade{ kDfciLift, kDfciContrast };

	if (From(entry.game, FbGameFolder::Game_UNIEL))
		return Grade{ kUnielLift, kUnielContrast };

	if (Arcsys(entry.game))
		return Grade{ kBbtagLift, kBbtagContrast };

	return Grade{ kGameLift, kGameContrast };
}

BgGrade::Grade BgGrade::Of(int stage)
{
	const std::map<int, Grade>::const_iterator known = g_cache.find(stage);

	if (known != g_cache.end())
		return known->second;

	const Grade measured = DefaultOf(stage);

	char stored[64] = {};

	GetPrivateProfileStringA(kSection, Key(stage).c_str(), "", stored, sizeof(stored),
		Settings::GetIniPath().c_str());

	float lift = 0.0f;
	float contrast = 0.0f;
	float wasLift = 0.0f;
	float wasContrast = 0.0f;
	const int read = sscanf_s(stored, "%f,%f,%f,%f", &lift, &contrast, &wasLift, &wasContrast);

	if (read == 4 && Same(wasLift, measured.lift) && Same(wasContrast, measured.contrast))
	{
		const Grade kept = { lift, contrast };

		g_cache[stage] = kept;
		return kept;
	}

	if (stored[0] != 0)
	{
		Settings::SaveString(kSection, Key(stage).c_str(), "");

		LOG("BgGrade: stage %d carried '%s', which was stored against a colour the mod no longer "
			"measures, so the stage takes the table's %.3f, %.3f instead", stage, stored,
			measured.lift, measured.contrast);
	}

	g_cache[stage] = measured;
	return measured;
}

void BgGrade::Set(int stage, const Grade& grade)
{
	const Grade measured = DefaultOf(stage);

	char value[64] = {};
	sprintf_s(value, "%.4f,%.4f,%.4f,%.4f", grade.lift, grade.contrast, measured.lift,
		measured.contrast);

	Settings::SaveString(kSection, Key(stage).c_str(), value);

	g_cache[stage] = grade;
	InterlockedExchange(&g_dirty, 1);
	FrameStepper::RequestRepaint();
}

void BgGrade::Forget(int stage)
{
	Settings::SaveString(kSection, Key(stage).c_str(), "");
	Settings::SaveString(kSection, (Key(stage) + "Glow").c_str(), "");

	g_cache.erase(stage);
	g_glowCache.erase(stage);

	InterlockedExchange(&g_dirty, 1);
}

void BgGrade::Update()
{
	if (InterlockedCompareExchange(&g_reached, 0, 0) == 0)
		return;

	Flow();
	Lamps();
	Assert();

	const uintptr_t address = RvaToAddress(GameOffsets::kBgPendingNumber);

	if (!IsAddressInGameModule(address))
		return;

	const int number = *reinterpret_cast<const int*>(address);
	const int loaded = StageLibrary::IdForSlot(number);
	const int stage = loaded < 0 ? number : loaded;
	const int held = g_stage;
	const long dirty = InterlockedExchange(&g_dirty, 0);

	if (stage == g_stage && dirty == 0)
		return;

	if (stage != g_stage)
	{
		LOG("BgGrade: the stage the game is holding changed to %d", number);
		g_cache.erase(stage);
	}

	g_stage = stage;

	const bool off = IsOff(stage);
	const Grade grade = off ? Grade{ kGameLift, kGameContrast } : Of(stage);
	g_glow = off ? kGameGlow : GlowOf(stage);

	for (int i = 0; i < 3; ++i)
	{
		g_live[kLiftFirst + i] = grade.lift;
		g_live[kContrastFirst + i] = grade.contrast;
	}

	const bool own = grade.lift == kGameLift && grade.contrast == kGameContrast;

	InterlockedExchange(&g_altered, own ? 0 : 1);

	if (own)
		Upload();

	if (stage != held)
	{
		static Note note;
		const bool read = ReadNote(stage, note);
		const bool flowing = read && note.flowing && Flows(stage);
		const bool fading = read && note.fading;

		for (int i = 0; i < kFlowSlots; ++i)
		{
			g_rate[i] = flowing ? note.rate[i] : 0.0f;
			g_live[kFlowFirst + i] = 0.0f;
		}

		const bool lighting = read && note.lighting;

		g_frame = 0;

		for (int i = 0; i < kLampSlots; ++i)
		{
			g_lamp[i] = lighting ? note.lamp[i] : Lamp{};
			g_live[kLampFirst + i] = 1.0f;
		}

		InterlockedExchange(&g_lighting, lighting ? 1 : 0);

		g_live[kLiftFirst + 3] = fading ? 1.0f : 0.0f;

		InterlockedExchange(&g_flowing, flowing ? 1 : 0);
		InterlockedExchange(&g_fading, fading ? 1 : 0);

		if (flowing)
			LOG("BgGrade: stage %d scrolls %d surface(s) from its own stage.txt", stage,
				static_cast<int>(std::count_if(g_rate, g_rate + kFlowSlots,
					[](float r) { return r != 0.0f; })));

		if (fading)
			LOG("BgGrade: stage %d asks for its vertex alpha, so the background shader takes "
				"it from the mesh as well as from the texture", stage);
	}
}

int BgGrade::Scrolling()
{
	if (InterlockedCompareExchange(&g_flowing, 0, 0) == 0)
		return 0;

	int live = 0;

	for (float rate : g_rate)
		live += rate != 0.0f ? 1 : 0;

	return live;
}

bool BgGrade::Reached()
{
	return InterlockedCompareExchange(&g_reached, 0, 0) != 0;
}

float BgGrade::DefaultGlowOf(int stage)
{
	StageLibrary::Entry entry = {};
	const bool ported = StageLibrary::Of(stage, entry) && Arcsys(entry.game);

	return ported ? kBbtagGlow : kGameGlow;
}

float BgGrade::GlowOf(int stage)
{
	const std::map<int, float>::const_iterator known = g_glowCache.find(stage);

	if (known != g_glowCache.end())
		return known->second;

	float glow = DefaultGlowOf(stage);

	char stored[32] = {};

	GetPrivateProfileStringA(kSection, (Key(stage) + "Glow").c_str(), "", stored, sizeof(stored),
		Settings::GetIniPath().c_str());

	if (stored[0] != 0)
	{
		const float kept = static_cast<float>(atof(stored));

		if (kept >= 0.0f && kept <= 4.0f)
			glow = kept;
	}

	g_glowCache[stage] = glow;

	return glow;
}

void BgGrade::SetGlow(int stage, float glow)
{
	if (glow < 0.0f || glow > 4.0f)
		return;

	g_glowCache[stage] = glow;

	char value[32] = {};
	sprintf_s(value, "%.3f", glow);
	Settings::SaveString(kSection, (Key(stage) + "Glow").c_str(), value);

	InterlockedExchange(&g_dirty, 1);
}

bool BgGrade::IsOff(int stage)
{
	const std::map<int, bool>::const_iterator known = g_offCache.find(stage);

	if (known != g_offCache.end())
		return known->second;

	const bool off = GetPrivateProfileIntA(kSection, (Key(stage) + "Off").c_str(), 0,
		Settings::GetIniPath().c_str()) != 0;

	g_offCache[stage] = off;

	return off;
}

void BgGrade::SetOff(int stage, bool off)
{
	g_offCache[stage] = off;

	Settings::SaveString(kSection, (Key(stage) + "Off").c_str(), off ? "1" : "");

	InterlockedExchange(&g_dirty, 1);
	FrameStepper::RequestRepaint();
}

const char* BgGrade::StatusText()
{
	return g_status;
}
