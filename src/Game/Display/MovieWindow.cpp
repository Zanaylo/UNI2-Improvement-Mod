#include "Game/Display/MovieWindow.h"

#include "Core/ComRef.h"
#include "Core/logger.h"
#include "Hooks/GameHook.h"

#include <Windows.h>
#include <dshow.h>

#include <atomic>
#include <cstdio>

namespace {

using CoCreateInstance_t = HRESULT(WINAPI*)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
using RenderFile_t = HRESULT(STDMETHODCALLTYPE*)(IGraphBuilder*, LPCWSTR, LPCWSTR);
using GetAvgTimePerFrame_t = HRESULT(STDMETHODCALLTYPE*)(IBasicVideo*, REFTIME*);

constexpr int kRenderFileIndex = 13;
constexpr int kGetAvgTimePerFrameIndex = 7;

constexpr GUID kFilterGraph = { 0xe436ebb3, 0x524f, 0x11ce, { 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 } };
constexpr GUID kNullRenderer = { 0xc1f400a4, 0x3f08, 0x11d3, { 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37 } };

GameHook<CoCreateInstance_t> g_coCreateHook("CoCreateInstance");
GameHook<RenderFile_t> g_renderFileHook("IGraphBuilder::RenderFile");
GameHook<GetAvgTimePerFrame_t> g_frameTimeHook("IBasicVideo::get_AvgTimePerFrame");

std::atomic<double> g_frameSeconds{ 0.0 };
std::atomic<bool> g_frameSecondsKnown{ false };

std::atomic<bool> g_graphSeen{ false };
volatile LONG g_renderersReplaced = 0;
volatile LONG g_replaceFailures = 0;

void** VTableOf(void* object)
{
	return *static_cast<void***>(object);
}

bool FindWindowedRenderer(IGraphBuilder* graph, IBaseFilter** outRenderer)
{
	ComRef<IEnumFilters> filters;
	if (FAILED(graph->EnumFilters(filters.Out())))
		return false;

	IBaseFilter* filter = nullptr;

	while (filters->Next(1, &filter, nullptr) == S_OK)
	{
		ComRef<IVideoWindow> window;

		if (SUCCEEDED(filter->QueryInterface(__uuidof(IVideoWindow), window.OutVoid())))
		{
			*outRenderer = filter;
			return true;
		}

		filter->Release();
	}

	return false;
}

bool FirstPin(IBaseFilter* filter, PIN_DIRECTION direction, IPin** outPin)
{
	ComRef<IEnumPins> pins;
	if (FAILED(filter->EnumPins(pins.Out())))
		return false;

	IPin* pin = nullptr;

	while (pins->Next(1, &pin, nullptr) == S_OK)
	{
		PIN_DIRECTION actual = PINDIR_INPUT;

		if (SUCCEEDED(pin->QueryDirection(&actual)) && actual == direction)
		{
			*outPin = pin;
			return true;
		}

		pin->Release();
	}

	return false;
}

bool ReadFrameSeconds(IGraphBuilder* graph, double& outSeconds)
{
	ComRef<IBasicVideo> video;
	if (!g_frameTimeHook.IsLive() || FAILED(graph->QueryInterface(__uuidof(IBasicVideo), video.OutVoid())))
		return false;

	return SUCCEEDED(g_frameTimeHook.Original()(video.Get(), &outSeconds));
}

bool ConnectNullRenderer(IGraphBuilder* graph, IPin* upstream)
{
	ComRef<IBaseFilter> nullRenderer;
	if (FAILED(g_coCreateHook.Original()(kNullRenderer, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IBaseFilter),
		nullRenderer.OutVoid())))
	{
		return false;
	}

	if (FAILED(graph->AddFilter(nullRenderer.Get(), L"Null Renderer")))
		return false;

	ComRef<IPin> input;

	if (FirstPin(nullRenderer.Get(), PINDIR_INPUT, input.Out()) &&
		SUCCEEDED(graph->ConnectDirect(upstream, input.Get(), nullptr)))
	{
		return true;
	}

	graph->RemoveFilter(nullRenderer.Get());
	return false;
}

bool ReplaceWindowedRenderer(IGraphBuilder* graph)
{
	ComRef<IBaseFilter> renderer;
	if (!FindWindowedRenderer(graph, renderer.Out()))
		return false;

	ComRef<IPin> input;
	ComRef<IPin> upstream;

	if (!FirstPin(renderer.Get(), PINDIR_INPUT, input.Out()) || FAILED(input->ConnectedTo(upstream.Out())))
		return false;

	double frameSeconds = 0.0;
	if (!ReadFrameSeconds(graph, frameSeconds))
		return false;

	if (FAILED(graph->RemoveFilter(renderer.Get())))
		return false;

	if (ConnectNullRenderer(graph, upstream.Get()))
	{
		g_frameSeconds.store(frameSeconds);
		g_frameSecondsKnown.store(true);
		return true;
	}

	graph->AddFilter(renderer.Get(), L"Video Renderer");
	graph->ConnectDirect(upstream.Get(), input.Get(), nullptr);
	return false;
}

HRESULT STDMETHODCALLTYPE HookedRenderFile(IGraphBuilder* graph, LPCWSTR file, LPCWSTR playList)
{
	const HRESULT result = g_renderFileHook.Original()(graph, file, playList);

	if (FAILED(result))
		return result;

	if (ReplaceWindowedRenderer(graph))
	{
		InterlockedIncrement(&g_renderersReplaced);
		LOG("[MovieWindow] %ls: the video renderer and its window were swapped for a null renderer",
			file != nullptr ? file : L"(movie)");
		return result;
	}

	InterlockedIncrement(&g_replaceFailures);
	LOG("[MovieWindow] %ls: the video renderer was left in place", file != nullptr ? file : L"(movie)");
	return result;
}

HRESULT STDMETHODCALLTYPE HookedGetAvgTimePerFrame(IBasicVideo* video, REFTIME* frameTime)
{
	const HRESULT result = g_frameTimeHook.Original()(video, frameTime);

	if (SUCCEEDED(result) || frameTime == nullptr || !g_frameSecondsKnown.load())
		return result;

	*frameTime = g_frameSeconds.load();
	return S_OK;
}

void WatchGraph(IUnknown* graph)
{
	if (g_graphSeen.exchange(true))
		return;

	ComRef<IGraphBuilder> builder;
	ComRef<IBasicVideo> video;

	if (FAILED(graph->QueryInterface(__uuidof(IGraphBuilder), builder.OutVoid())) ||
		FAILED(graph->QueryInterface(__uuidof(IBasicVideo), video.OutVoid())))
	{
		LOG("[MovieWindow] a filter graph was created without IGraphBuilder or IBasicVideo");
		return;
	}

	if (!g_frameTimeHook.Install(VTableOf(video.Get())[kGetAvgTimePerFrameIndex], &HookedGetAvgTimePerFrame))
	{
		LOG("[MovieWindow] IBasicVideo::get_AvgTimePerFrame could not be hooked, the movie window is left alone");
		return;
	}

	if (!g_renderFileHook.Install(VTableOf(builder.Get())[kRenderFileIndex], &HookedRenderFile))
		LOG("[MovieWindow] IGraphBuilder::RenderFile could not be hooked");
}

HRESULT WINAPI HookedCoCreateInstance(REFCLSID classId, LPUNKNOWN outer, DWORD context, REFIID interfaceId,
	LPVOID* object)
{
	const HRESULT result = g_coCreateHook.Original()(classId, outer, context, interfaceId, object);

	if (SUCCEEDED(result) && object != nullptr && *object != nullptr && IsEqualCLSID(classId, kFilterGraph))
		WatchGraph(static_cast<IUnknown*>(*object));

	return result;
}

}

void MovieWindow::Install()
{
	if (!g_coCreateHook.InstallApi("ole32.dll", "CoCreateInstance", &HookedCoCreateInstance))
		LOG("[MovieWindow] CoCreateInstance could not be hooked, the movie window is left alone");
}

const char* MovieWindow::StatusText()
{
	static char text[96];
	sprintf_s(text, "%ld movie window(s) prevented, %ld left in place", g_renderersReplaced, g_replaceFailures);
	return text;
}
