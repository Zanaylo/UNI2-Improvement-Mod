#include "Core/SpikeWatch.h"

#include "Core/Config/interfaces.h"
#include "Core/Profiler.h"
#include "Core/ThreadRole.h"
#include "Core/logger.h"
#include "Hooks/GameHook.h"

#include <Windows.h>
#include <d3d9.h>

#include <atomic>
#include <cstdio>
#include <string>

namespace {

constexpr int kCreateVertexBufferIndex = 26;
constexpr int kCreateIndexBufferIndex = 27;
constexpr int kCreateVertexShaderIndex = 91;
constexpr int kCreatePixelShaderIndex = 106;

using ReadFile_t = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
using CreateVertexBuffer_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, DWORD, DWORD, D3DPOOL,
	IDirect3DVertexBuffer9**, HANDLE*);
using CreateIndexBuffer_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, DWORD, D3DFORMAT, D3DPOOL,
	IDirect3DIndexBuffer9**, HANDLE*);
using CreateVertexShader_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**);
using CreatePixelShader_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**);

GameHook<ReadFile_t> g_readFileHook("ReadFile");
GameHook<CreateVertexBuffer_t> g_vertexBufferHook("IDirect3DDevice9::CreateVertexBuffer");
GameHook<CreateIndexBuffer_t> g_indexBufferHook("IDirect3DDevice9::CreateIndexBuffer");
GameHook<CreateVertexShader_t> g_vertexShaderHook("IDirect3DDevice9::CreateVertexShader");
GameHook<CreatePixelShader_t> g_pixelShaderHook("IDirect3DDevice9::CreatePixelShader");

struct Counter
{
	std::atomic<int64_t> ticks{ 0 };
	std::atomic<uint32_t> calls{ 0 };
	std::atomic<uint64_t> bytes{ 0 };
};

Counter g_counters[SpikeReport::Cost_COUNT];

int64_t g_modTicks = 0;
int64_t g_slowestTaskTicks = 0;
const char* g_slowestGroup = nullptr;
int g_slowestIndex = 0;
int64_t g_lastPresent = 0;
SpikeReport::Limiter g_limiter;

bool IsGameThread()
{
	const ThreadRole::Role role = ThreadRole::Current();

	return role == ThreadRole::Role_Game || role == ThreadRole::Role_Render;
}

BOOL WINAPI HookedReadFile(HANDLE file, LPVOID buffer, DWORD wanted, LPDWORD read, LPOVERLAPPED overlapped)
{
	const int64_t began = Profiler::Now();
	DWORD got = 0;
	LPDWORD out = read != nullptr ? read : (overlapped == nullptr ? &got : nullptr);

	const BOOL result = g_readFileHook.Original()(file, buffer, wanted, out, overlapped);
	const uint64_t bytes = out != nullptr ? *out : 0;

	SpikeWatch::Add(IsGameThread() ? SpikeReport::Cost_GameFileRead : SpikeReport::Cost_OtherFileRead,
		Profiler::Now() - began, bytes);

	return result;
}

template <typename Call>
HRESULT Timed(SpikeReport::Cost cost, Call call)
{
	const int64_t began = Profiler::Now();
	const HRESULT result = call();
	SpikeWatch::Add(cost, Profiler::Now() - began);

	return result;
}

HRESULT STDMETHODCALLTYPE HookedCreateVertexBuffer(IDirect3DDevice9* device, UINT length, DWORD usage, DWORD fvf,
	D3DPOOL pool, IDirect3DVertexBuffer9** out, HANDLE* shared)
{
	return Timed(SpikeReport::Cost_BufferCreate,
		[&]() { return g_vertexBufferHook.Original()(device, length, usage, fvf, pool, out, shared); });
}

HRESULT STDMETHODCALLTYPE HookedCreateIndexBuffer(IDirect3DDevice9* device, UINT length, DWORD usage,
	D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** out, HANDLE* shared)
{
	return Timed(SpikeReport::Cost_BufferCreate,
		[&]() { return g_indexBufferHook.Original()(device, length, usage, format, pool, out, shared); });
}

HRESULT STDMETHODCALLTYPE HookedCreateVertexShader(IDirect3DDevice9* device, const DWORD* code,
	IDirect3DVertexShader9** out)
{
	return Timed(SpikeReport::Cost_ShaderCreate, [&]() { return g_vertexShaderHook.Original()(device, code, out); });
}

HRESULT STDMETHODCALLTYPE HookedCreatePixelShader(IDirect3DDevice9* device, const DWORD* code,
	IDirect3DPixelShader9** out)
{
	return Timed(SpikeReport::Cost_ShaderCreate, [&]() { return g_pixelShaderHook.Original()(device, code, out); });
}

template <typename Fn, typename Handler>
void HookSlot(void** vtable, int index, GameHook<Fn>& hook, Handler handler)
{
	if (hook.IsLive() || hook.Install(vtable[index], handler))
		return;

	LOG("SpikeWatch: could not hook %s", hook.Label());
}

SpikeReport::Spent Take(Counter& counter)
{
	SpikeReport::Spent spent = {};
	spent.ms = Profiler::ToMs(counter.ticks.exchange(0));
	spent.calls = counter.calls.exchange(0);
	spent.bytes = counter.bytes.exchange(0);

	return spent;
}

SpikeReport::Frame TakeFrame(double intervalMs, char* task, size_t taskSize)
{
	SpikeReport::Frame frame = {};
	frame.intervalMs = intervalMs;

	for (int cost = 0; cost < SpikeReport::Cost_COUNT; ++cost)
		frame.costs[cost] = Take(g_counters[cost]);

	sprintf_s(task, taskSize, "%s#%d", g_slowestGroup != nullptr ? g_slowestGroup : "?", g_slowestIndex);
	frame.modMs = Profiler::ToMs(g_modTicks);
	frame.slowestTask = task;
	frame.slowestTaskMs = Profiler::ToMs(g_slowestTaskTicks);

	g_modTicks = 0;
	g_slowestTaskTicks = 0;
	g_slowestGroup = nullptr;

	return frame;
}

}

bool SpikeWatch::Install()
{
	if (g_readFileHook.InstallApi("kernel32.dll", "ReadFile", &HookedReadFile))
		return true;

	LOG("SpikeWatch: ReadFile could not be hooked, file reads are not timed");
	return false;
}

void SpikeWatch::AttachDevice(void** vtable)
{
	if (vtable == nullptr)
		return;

	HookSlot(vtable, kCreateVertexBufferIndex, g_vertexBufferHook, &HookedCreateVertexBuffer);
	HookSlot(vtable, kCreateIndexBufferIndex, g_indexBufferHook, &HookedCreateIndexBuffer);
	HookSlot(vtable, kCreateVertexShaderIndex, g_vertexShaderHook, &HookedCreateVertexShader);
	HookSlot(vtable, kCreatePixelShaderIndex, g_pixelShaderHook, &HookedCreatePixelShader);
}

void SpikeWatch::Add(SpikeReport::Cost cost, int64_t ticks, uint64_t bytes)
{
	if (cost < 0 || cost >= SpikeReport::Cost_COUNT)
		return;

	Counter& counter = g_counters[cost];
	counter.ticks.fetch_add(ticks, std::memory_order_relaxed);
	counter.calls.fetch_add(1, std::memory_order_relaxed);
	counter.bytes.fetch_add(bytes, std::memory_order_relaxed);
}

void SpikeWatch::NoteTask(const char* group, int index, int64_t ticks)
{
	g_modTicks += ticks;

	if (ticks <= g_slowestTaskTicks)
		return;

	g_slowestTaskTicks = ticks;
	g_slowestGroup = group;
	g_slowestIndex = index;
}

void SpikeWatch::OnPresent()
{
	const int64_t now = Profiler::Now();
	const int64_t last = g_lastPresent;
	g_lastPresent = now;

	char task[32] = {};
	SpikeReport::Frame frame = TakeFrame(last == 0 ? 0.0 : Profiler::ToMs(now - last), task, sizeof(task));

	if (!SpikeReport::IsSpike(frame.intervalMs, g_modVals.spikeReportMs) || !g_limiter.Allow(GetTickCount()))
		return;

	frame.suppressed = g_limiter.TakeSuppressed();
	LOG("%s", SpikeReport::Describe(frame).c_str());
}
