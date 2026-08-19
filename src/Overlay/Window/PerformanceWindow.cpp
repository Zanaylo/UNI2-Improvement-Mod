#include "Overlay/Window/PerformanceWindow.h"

#include "Core/ProcessTuning.h"
#include "Core/Profiler.h"
#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/utils.h"
#include "D3D9/DeviceHooks.h"
#include "D3D9/PresentTuning.h"
#include "D3D9/RenderScale.h"
#include "Game/PotatoMode.h"
#include "Game/PumpWait.h"
#include "Training/InputLagMeter.h"
#include "Training/StageColor.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace {

// POTATO MODE is finished and compiled in, but held back from the beta: the internal resolution it
// turns down has never been verified in game, and a wrong answer there puts the scene in a corner of
// the screen. Flip this to true to get the tab back; the settings under [Graphics] still work if
// they are written into the ini by hand.
constexpr bool kShowPotatoTab = false;

constexpr float kHistogramHeight = 110.0f;
constexpr float kDefaultWidth = 780.0f;
constexpr float kDefaultHeight = 600.0f;

const ImVec4 kGoodColour = ImVec4(0.45f, 0.80f, 0.50f, 1.0f);
const ImVec4 kWarnColour = ImVec4(0.95f, 0.55f, 0.45f, 1.0f);

struct Option
{
	bool* value;
	const char* key;
	const char* label;
	const char* summary;
	const char* help;
};

const Option kOptions[] = {
	{
		&g_modVals.timerResolution,
		"TimerResolution",
		"Keep the Windows timer at 1 ms",
		"Fixes what alt-tab leaves behind. No downside.",
		"The game asks Windows for 1 ms timer resolution once, at startup, and never again. Since "
		"Windows 10 2004 that request is per process and Windows takes it back while the process "
		"sits in the background, so after an alt-tab every sleep in the engine can last 15.6 ms "
		"instead of 1. This holds the request and asks again when the window comes back.",
	},
	{
		&g_modVals.powerThrottlingOptOut,
		"PowerThrottlingOptOut",
		"Stop Windows throttling the game in the background",
		"The other half of the same fix. Costs a little power out of focus.",
		"EcoQoS parks a background process on the efficient cores and clamps its timer resolution. "
		"Opting out keeps the game on the fast cores and keeps the millisecond timer above.",
	},
	{
		&g_modVals.pumpWait,
		"PumpWait",
		"Wake the input thread the moment the game asks it",
		"Removes the waiting, and fixes what alt-tab leaves behind. No CPU cost.",
		"The game runs its message pump on one thread and its frame on another, and every frame the "
		"frame thread blocks on a SendMessage that only the pump can answer - it exists so the "
		"keyboard is read on the thread that owns the input queue. Sleep does not service a sent "
		"message, so that handshake waits out the pump whole Sleep(1), and fifteen milliseconds of "
		"one after an alt-tab has clamped the timer.\n\n"
		"This replaces that sleep with a wait that also wakes on a sent message. It costs no CPU, "
		"patches no engine code, and switches off cleanly.\n\n"
		"It matters most in exclusive fullscreen and after an alt-tab, where Windows clamps the "
		"timer and that one millisecond becomes fifteen.",
	},
};

void Help(const char* text)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");

	if (!ImGui::IsItemHovered())
		return;

	ImGui::BeginTooltip();
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34.0f);
	ImGui::TextUnformatted(text);
	ImGui::PopTextWrapPos();
	ImGui::EndTooltip();
}

void Muted(const char* format, ...)
{
	char text[768] = {};

	va_list args;
	va_start(args, format);
	vsprintf_s(text, format, args);
	va_end(args);

	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrapped("%s", text);
	ImGui::PopStyleColor();
}

void Warn(const char* format, ...)
{
	char text[512] = {};

	va_list args;
	va_start(args, format);
	vsprintf_s(text, format, args);
	va_end(args);

	ImGui::PushStyleColor(ImGuiCol_Text, kWarnColour);
	ImGui::TextWrapped("%s", text);
	ImGui::PopStyleColor();
}

void SaveVideo(const char* key, int value)
{
	Settings::SaveInt("Video", key, value);
}

void DeltaText(double now, double before)
{
	if (before <= 0.0)
	{
		ImGui::TextDisabled("-");
		return;
	}

	const double delta = now - before;
	const ImVec4 colour = delta == 0.0 ? ImVec4(0.7f, 0.7f, 0.7f, 1.0f)
		: delta < 0.0 ? kGoodColour : kWarnColour;

	ImGui::TextColored(colour, "%+.2f ms (%+.0f%%)", delta, delta / before * 100.0);
}

void StatRow(const char* name, double now, double before, bool hasBaseline)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(name);

	ImGui::TableNextColumn();
	if (hasBaseline)
		ImGui::Text("%.2f ms", before);
	else
		ImGui::TextDisabled("-");

	ImGui::TableNextColumn();
	ImGui::Text("%.2f ms", now);

	ImGui::TableNextColumn();
	if (hasBaseline)
		DeltaText(now, before);
	else
		ImGui::TextDisabled("-");
}

void DrawIntervalTable(const char* title, const Profiler::Stats& now, const Profiler::Stats& before,
	bool hasBaseline, bool withSpread)
{
	ImGui::SeparatorText(title);

	if (!ImGui::BeginTable(title, 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("What");
	ImGui::TableSetupColumn("Baseline");
	ImGui::TableSetupColumn("Now");
	ImGui::TableSetupColumn("Change");
	ImGui::TableHeadersRow();

	StatRow("Median", now.medianMs, before.medianMs, hasBaseline);

	if (withSpread)
	{
		StatRow("Spread (standard deviation)", now.stddevMs, before.stddevMs, hasBaseline);
		StatRow("Off target, average", now.madMs, before.madMs, hasBaseline);
	}

	StatRow("99th percentile", now.p99Ms, before.p99Ms, hasBaseline);
	StatRow("Worst", now.maxMs, before.maxMs, hasBaseline);
	StatRow("Average", now.averageMs, before.averageMs, hasBaseline);

	if (withSpread)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Within half a ms of 16.67");
		ImGui::TableNextColumn();

		if (hasBaseline)
			ImGui::Text("%.0f%%", before.onTargetPercent);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		ImGui::Text("%.0f%%", now.onTargetPercent);
		ImGui::TableNextColumn();

		if (hasBaseline)
		{
			const double delta = now.onTargetPercent - before.onTargetPercent;
			ImGui::TextColored(delta >= 0.0 ? kGoodColour : kWarnColour, "%+.0f%%", delta);
		}
		else
		{
			ImGui::TextDisabled("-");
		}
	}

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted("Frames over 20 ms");
	ImGui::TableNextColumn();

	if (hasBaseline)
		ImGui::Text("%d of %d", before.slowFrames, before.samples);
	else
		ImGui::TextDisabled("-");

	ImGui::TableNextColumn();
	ImGui::Text("%d of %d", now.slowFrames, now.samples);
	ImGui::TableNextColumn();
	ImGui::TextDisabled("-");

	ImGui::EndTable();
}

void DrawHistogram()
{
	float values[Profiler::kHistogramBuckets] = {};
	float peak = 1.0f;

	for (int i = 0; i < Profiler::kHistogramBuckets; ++i)
	{
		values[i] = static_cast<float>(Profiler::GetHistogramBucket(i));

		if (values[i] > peak)
			peak = values[i];
	}

	ImGui::PlotHistogram("##frameinterval", values, Profiler::kHistogramBuckets, 0, nullptr, 0.0f,
		peak, ImVec2(-1.0f, kHistogramHeight));

	Muted("Every frame since the last reset, in 1 ms buckets from 0 to 40 ms. This is where a "
		"dropped frame shows up, as a tail to the right.");
}

void DrawFineHistogram()
{
	float values[Profiler::kFineBuckets] = {};
	float peak = 1.0f;

	for (int i = 0; i < Profiler::kFineBuckets; ++i)
	{
		values[i] = static_cast<float>(Profiler::GetFineHistogramBucket(i));

		if (values[i] > peak)
			peak = values[i];
	}

	ImGui::PlotHistogram("##fineinterval", values, Profiler::kFineBuckets, 0, nullptr, 0.0f, peak,
		ImVec2(-1.0f, kHistogramHeight));

	const double base = Profiler::GetFineHistogramBaseMs();
	Muted("The same frames in quarter-millisecond buckets from %.2f to %.2f ms. Smooth is one spike "
		"in the middle. Two spikes either side of it is judder, and it is invisible to the median - "
		"which is exactly how it went unnoticed.",
		base, base + Profiler::kFineBuckets * Profiler::kFineBucketMs);

	double firstMs = 0.0;
	double secondMs = 0.0;
	double separationMs = 0.0;

	if (Profiler::FindModes(firstMs, secondMs, separationMs))
	{
		Warn("Two clusters, %.2f ms and %.2f ms, %.2f ms apart. Frames are landing either side of "
			"the target rather than on it.", firstMs, secondMs, separationMs);
	}
}

void DrawSections(bool hasBaseline)
{
	ImGui::SeparatorText("Where the time in a frame goes");

	Muted("The mod's own work, in milliseconds, averaged over recent frames. The two named after "
		"the game's functions - oPresent and oFrameUpdate - are the game itself, for comparison.");

	if (!ImGui::BeginTable("##sections", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("Section");
	ImGui::TableSetupColumn("Baseline");
	ImGui::TableSetupColumn("Now");
	ImGui::TableSetupColumn("Change");
	ImGui::TableHeadersRow();

	for (int i = 0; i < Profiler::Section_COUNT; ++i)
	{
		const Profiler::Section section = static_cast<Profiler::Section>(i);
		const double now = Profiler::GetSectionMs(section);
		const double before = Profiler::GetBaselineSectionMs(section);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Profiler::GetSectionName(section));

		ImGui::TableNextColumn();
		if (hasBaseline)
			ImGui::Text("%.3f", before);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		ImGui::Text("%.3f", now);

		ImGui::TableNextColumn();
		if (hasBaseline && before > 0.0)
			DeltaText(now, before);
		else
			ImGui::TextDisabled("-");
	}

	ImGui::EndTable();
}

void BuildBaselineLabel(char* out, size_t size)
{
	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();

	sprintf_s(out, size, "%s %u Hz, %u buffer(s), vsync %s, timer %d, throttling %d, pump %d",
		present.Windowed ? "windowed" : "fullscreen",
		present.FullScreen_RefreshRateInHz, present.BackBufferCount,
		present.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE ? "off" : "on",
		g_modVals.timerResolution ? 1 : 0, g_modVals.powerThrottlingOptOut ? 1 : 0,
		g_modVals.pumpWait ? 1 : 0);
}

void ApplyPreset(bool timer, bool throttling, bool pump)
{
	g_modVals.timerResolution = timer;
	g_modVals.powerThrottlingOptOut = throttling;
	g_modVals.pumpWait = pump;

	g_modVals.displayTuning = true;
	g_modVals.extraBackBuffer = false;
	g_modVals.fullscreenRefreshHz = 0;

	SaveVideo("TimerResolution", timer ? 1 : 0);
	SaveVideo("PowerThrottlingOptOut", throttling ? 1 : 0);
	SaveVideo("PumpWait", pump ? 1 : 0);
	SaveVideo("DisplayTuning", 1);
	SaveVideo("ExtraBackBuffer", 0);
	SaveVideo("FullscreenRefreshHz", 0);
}

bool VsyncIsOn(const D3DPRESENT_PARAMETERS& present)
{
	return present.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE;
}

// Enumerating adapter modes is a pile of real Direct3D calls, and the interval readouts each
// sort a 240 sample window. Doing any of that once a frame is what dropped the frame rate while
// this window was open; none of it is worth more than a few updates a second.
constexpr DWORD kRefreshIntervalMs = 250;

bool ShouldRefresh(DWORD& lastTick)
{
	const DWORD now = GetTickCount();

	if (lastTick != 0 && now - lastTick < kRefreshIntervalMs)
		return false;

	lastTick = now;
	return true;
}

void CollectRefreshRates(std::vector<UINT>& out)
{
	out.clear();

	IDirect3DDevice9* const device = DeviceHooks::GetDevice();
	if (device == nullptr)
		return;

	IDirect3D9* d3d9 = nullptr;
	if (FAILED(device->GetDirect3D(&d3d9)) || d3d9 == nullptr)
		return;

	D3DDEVICE_CREATION_PARAMETERS creation = {};
	const UINT adapter = SUCCEEDED(device->GetCreationParameters(&creation))
		? creation.AdapterOrdinal : D3DADAPTER_DEFAULT;

	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	const UINT modeCount = d3d9->GetAdapterModeCount(adapter, present.BackBufferFormat);

	for (UINT i = 0; i < modeCount; ++i)
	{
		D3DDISPLAYMODE mode = {};
		if (FAILED(d3d9->EnumAdapterModes(adapter, present.BackBufferFormat, i, &mode)))
			continue;

		if (mode.Width != present.BackBufferWidth || mode.Height != present.BackBufferHeight)
			continue;

		bool seen = false;
		for (const UINT rate : out)
			seen = seen || rate == mode.RefreshRate;

		if (!seen && mode.RefreshRate != 0)
			out.push_back(mode.RefreshRate);
	}

	d3d9->Release();
}

}

PerformanceWindow::PerformanceWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void PerformanceWindow::BeforeDraw()
{
	const ImGuiViewport* const viewport = ImGui::GetMainViewport();

	const float width = kDefaultWidth * g_modVals.uiScale;
	const float height = kDefaultHeight * g_modVals.uiScale;

	ImGui::SetNextWindowSize(ImVec2(width < viewport->WorkSize.x ? width : viewport->WorkSize.x,
		height < viewport->WorkSize.y ? height : viewport->WorkSize.y), ImGuiCond_FirstUseEver);

	ImGui::SetNextWindowSizeConstraints(ImVec2(440.0f, 280.0f), viewport->WorkSize);
}

void PerformanceWindow::Draw()
{
	if (!ImGui::BeginTabBar("##performance"))
		return;

	if (ImGui::BeginTabItem("Performance"))
	{
		DrawPerformanceTab();
		ImGui::EndTabItem();
	}

	if (kShowPotatoTab && ImGui::BeginTabItem("POTATO MODE"))
	{
		DrawPotatoTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Metrics"))
	{
		DrawMetricsTab();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

// Everything here is read back from the device and from the engine. Nothing on this panel is the
// mod repeating what it asked for - the point of it is to say when a request was refused.
void PerformanceWindow::DrawWhatIsHappening()
{
	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();

	if (present.BackBufferWidth == 0)
	{
		Warn("No Direct3D device found. Something else is wrapping Direct3D, and none of the "
			"display settings below are in force.");
		return;
	}

	ImGui::Text("%ux%u %s, %s, %u back buffer(s)", present.BackBufferWidth, present.BackBufferHeight,
		present.Windowed ? "windowed" : "exclusive fullscreen",
		VsyncIsOn(present) ? "vsync on" : "vsync off", present.BackBufferCount);

	if (!present.Windowed)
	{
		ImGui::SameLine();
		ImGui::Text("at %u Hz", present.FullScreen_RefreshRateInHz);
	}

	static DWORD stateTick = 0;
	static double fps = 0.0;

	if (ShouldRefresh(stateTick))
		fps = Profiler::GetPresentedFps();

	if (fps > 0.0)
		ImGui::Text("Presenting %.1f frames a second", fps);

	if (present.Windowed)
	{
		Muted("Windowed, the desktop compositor owns the presentation, so the display settings "
			"below do nothing. The game's own Video menu is where fullscreen is chosen.");
	}

	if (!present.Windowed && VsyncIsOn(present) && present.FullScreen_RefreshRateInHz % 60 != 0)
	{
		Warn("%u Hz cannot show 60 frames a second evenly, so with vsync on they land alternately "
			"early and late. Pick a refresh below that divides by 60, or turn the game's vsync off.",
			present.FullScreen_RefreshRateInHz);
	}

}

bool PerformanceWindow::DrawOptions()
{
	bool changed = false;

	for (const Option& option : kOptions)
	{
		ImGui::PushID(option.key);

		if (ImGui::Checkbox(option.label, option.value))
		{
			SaveVideo(option.key, *option.value ? 1 : 0);
			changed = true;
		}

		Help(option.help);

		ImGui::Indent();
		Muted("%s", option.summary);
		ImGui::Unindent();

		ImGui::PopID();
	}

	return changed;
}

bool PerformanceWindow::DrawDisplayGroup()
{
	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	const bool windowed = present.Windowed != FALSE;
	const bool vsync = VsyncIsOn(present);

	if (!ImGui::CollapsingHeader("Display"))
		return false;

	bool changed = false;

	ImGui::BeginDisabled(windowed);

	if (ImGui::Checkbox("Let the mod choose the display parameters", &g_modVals.displayTuning))
	{
		SaveVideo("DisplayTuning", g_modVals.displayTuning ? 1 : 0);
		changed = true;
	}

	Help("The game asks Direct3D for a hard-coded 60 Hz it never checks the adapter for, and for a "
		"single back buffer. Off leaves both exactly as the game asked.");

	static std::vector<UINT> rates;
	static DWORD ratesTick = 0;

	if (ShouldRefresh(ratesTick))
		CollectRefreshRates(rates);

	char preview[64] = {};
	if (g_modVals.fullscreenRefreshHz == 0)
		sprintf_s(preview, "Automatic - leave the desktop's own mode");
	else
		sprintf_s(preview, "%d Hz", g_modVals.fullscreenRefreshHz);

	ImGui::BeginDisabled(!g_modVals.displayTuning);

	ImGui::SetNextItemWidth(320.0f);

	if (ImGui::BeginCombo("Fullscreen refresh", preview))
	{
		if (ImGui::Selectable("Automatic - leave the desktop's own mode",
			g_modVals.fullscreenRefreshHz == 0))
		{
			g_modVals.fullscreenRefreshHz = 0;
			SaveVideo("FullscreenRefreshHz", 0);
			changed = true;
		}

		for (const UINT rate : rates)
		{
			char label[32] = {};
			sprintf_s(label, "%u Hz%s", rate, rate % 60 == 0 ? " (divides by 60)" : "");

			if (ImGui::Selectable(label, g_modVals.fullscreenRefreshHz == static_cast<int>(rate)))
			{
				g_modVals.fullscreenRefreshHz = static_cast<int>(rate);
				SaveVideo("FullscreenRefreshHz", g_modVals.fullscreenRefreshHz);
				changed = true;
			}
		}

		ImGui::EndCombo();
	}

	Help("With the game's vsync off the engine paces itself and Present never waits, so the refresh "
		"rate only decides how long a frame takes to reach the glass - and leaving the desktop's own "
		"mode alone is both the fastest and the cheapest to alt-tab out of.\n\n"
		"With vsync on and a rate that does not divide by 60, frames land alternately early and "
		"late. Automatic then picks the highest listed rate that does divide by 60.\n\n"
		"Restart the game to apply.");

	ImGui::BeginDisabled(!vsync);

	if (ImGui::Checkbox("A second back buffer", &g_modVals.extraBackBuffer))
	{
		SaveVideo("ExtraBackBuffer", g_modVals.extraBackBuffer ? 1 : 0);
		changed = true;
	}

	Help("The game asks for one. A second gives a frame that misses the vblank somewhere to wait "
		"instead of costing a whole refresh - and costs up to a frame of input latency to get.\n\n"
		"It only does anything with vsync on. With vsync off there is no vblank to miss, so the "
		"buffer becomes a queued frame of delay and nothing else. An earlier build of this mod "
		"turned it on for everybody by default, which is what made the game feel worse.");

	ImGui::EndDisabled();

	if (!vsync)
	{
		ImGui::Indent();
		Muted("Greyed out because the game's vsync is off, where a second buffer is pure latency. "
			"Turn vsync on in the game's own Video menu to make this available.");
		ImGui::Unindent();
	}

	ImGui::EndDisabled();
	ImGui::EndDisabled();

	if (windowed)
	{
		Muted("Greyed out because the game is windowed. Direct3D requires a zero refresh rate "
			"there, and the compositor already holds a frame of its own.");
	}

	return changed;
}

bool PerformanceWindow::DrawAdvanced()
{
	if (!ImGui::CollapsingHeader("Advanced"))
		return false;

	bool changed = false;

	if (ImGui::Checkbox("Wake the input thread on every message, not only the handshake",
		&g_modVals.pumpWaitAllInput))
	{
		SaveVideo("PumpWaitAllInput", g_modVals.pumpWaitAllInput ? 1 : 0);
		changed = true;
	}

	Help("The pump takes one message per pass, so a busy queue drains a message a millisecond. This "
		"drains it as fast as it fills, which shortens window and overlay message latency and costs "
		"CPU in proportion to how much the mouse moves. Needs the option above it switched on.");

	return changed;
}

bool PerformanceWindow::DrawPresets()
{
	ImGui::SeparatorText("Presets");

	bool changed = false;

	if (ImGui::Button("Recommended"))
	{
		ApplyPreset(true, true, true);
		changed = true;
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Everything above on, the display left as the desktop has it, and the "
			"game's own single back buffer. Nothing here costs a core.");
	}

	ImGui::SameLine();

	if (ImGui::Button("As the game ships"))
	{
		ApplyPreset(false, false, false);
		g_modVals.displayTuning = false;
		SaveVideo("DisplayTuning", 0);
		changed = true;
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Everything off. This is the thing to measure against.");

	return changed;
}

void PerformanceWindow::DrawPerformanceTab()
{
	ImGui::Spacing();
	ImGui::SeparatorText("What is actually happening");
	DrawWhatIsHappening();

	ImGui::Spacing();
	ImGui::SeparatorText("Choices");

	bool changed = DrawOptions();

	ImGui::Spacing();
	changed = DrawDisplayGroup() || changed;

	ImGui::Spacing();
	changed = DrawAdvanced() || changed;

	ImGui::Spacing();
	changed = DrawPresets() || changed;

	if (!changed)
		return;

	ProcessTuning::Apply();
	PumpWait::Apply();
	Profiler::Reset();
}

void PerformanceWindow::DrawMetricsTab()
{
	bool measuring = Profiler::IsEnabled();

	if (ImGui::Checkbox("Measure", &measuring))
	{
		g_modVals.profilerEnabled = measuring;
		Profiler::SetEnabled(measuring);
		Settings::SaveInt("Debug", "Profiler", measuring ? 1 : 0);
	}

	Help("Times the gap between finished frames, how long Present blocks, and every step of the "
		"mod's own work. Off by default so a session nobody is measuring pays nothing for it.");

	if (!measuring)
	{
		ImGui::Spacing();
		Muted("Nothing is being measured. Switch Measure on and play for a few seconds.");
		return;
	}

	ImGui::Spacing();

	const bool hasBaseline = Profiler::HasBaseline();

	// Every one of these sorts its own 240 sample window, so they are taken a few times a second
	// rather than once a frame - this tab used to cost the game frames just by being open.
	static DWORD metricsTick = 0;
	static double metricsFps = 0.0;
	static Profiler::Stats framePresent = {};
	static Profiler::Stats frameBaseline = {};
	static Profiler::Stats blockStats = {};
	static Profiler::Stats tickStats = {};
	static Profiler::Stats tickBaseline = {};

	if (ShouldRefresh(metricsTick))
	{
		metricsFps = Profiler::GetPresentedFps();
		framePresent = Profiler::GetPresentStats();
		frameBaseline = Profiler::GetBaselinePresentStats();
		blockStats = Profiler::GetPresentBlockStats();
		tickStats = Profiler::GetTickStats();
		tickBaseline = Profiler::GetBaselineTickStats();
	}

	ImGui::Text("Presenting %.1f frames a second", metricsFps);

	ImGui::Spacing();

	DrawIntervalTable("Frame interval", framePresent, frameBaseline, hasBaseline, true);

	Muted("The gap between one finished frame and the next, which is what smoothness is. Median is "
		"the typical frame; the spread and the off-target average are the judder. Frames over 20 ms "
		"are dropped frames.");

	ImGui::Spacing();
	ImGui::SeparatorText("Frame interval, close up");
	DrawFineHistogram();

	ImGui::Spacing();
	ImGui::SeparatorText("Frame interval, whole range");
	DrawHistogram();

	ImGui::Spacing();

	DrawIntervalTable("Present", blockStats, Profiler::Stats(), false, false);

	Muted("How long the call that hands the frame to the driver takes. With vsync on this is the "
		"wait for the vblank, and two clusters here are a display that cannot divide 60 evenly.");

	ImGui::Spacing();

	DrawIntervalTable("Battle tick", tickStats, tickBaseline, hasBaseline, false);

	Muted("The gap between two runs of the simulation. Only runs during a match, and reads zero "
		"while frame stepping has the game paused.");

	ImGui::Spacing();
	ImGui::SeparatorText("Input lag");

	const float lastLag = InputLagMeter::GetLastMs();
	const float averageLag = InputLagMeter::GetAverageMs();
	const int trusted = InputLagMeter::GetTrustedCount();

	if (averageLag > 0.0f)
	{
		ImGui::Text("%.1f ms average over %d trusted samples (%.1f frames), last %.1f ms",
			averageLag, trusted, averageLag / (1000.0f / 60.0f), lastLag);
	}
	else
	{
		ImGui::TextDisabled("Open Player Control and press something to measure.");
	}

	Muted("Measured from a physical press to the character's own input field changing, so it is the "
		"half of the delay the simulation can see. It cannot see the swap chain or the scan-out - "
		"Direct3D 9 without the Ex interfaces reports neither - so the display half below is "
		"arithmetic, not a measurement.");

	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	const double refresh = present.FullScreen_RefreshRateInHz != 0
		? 1000.0 / present.FullScreen_RefreshRateInHz : 1000.0 / 60.0;

	ImGui::Text("Display side, estimated: %.1f ms", blockStats.medianMs +
		present.BackBufferCount * refresh);

	ImGui::Spacing();
	DrawSections(hasBaseline);

	ImGui::Spacing();
	ImGui::SeparatorText("Before and after");

	Muted("Turn Measure on. Go to training mode, same character, same corner. Play thirty seconds. "
		"Press Capture baseline. Change one thing on the Performance tab. Play the same thirty "
		"seconds. Read the Change column, then Copy summary and paste it into the bug report.");

	if (hasBaseline)
		ImGui::Text("Baseline: %s", Profiler::GetBaselineLabel());
	else
		ImGui::TextDisabled("No baseline captured yet.");

	ImGui::Text("Live sample: %d frames", Profiler::GetSampleCount());

	if (ImGui::Button("Capture baseline"))
	{
		char label[192] = {};
		BuildBaselineLabel(label, sizeof(label));
		Profiler::CaptureBaseline(label);
		Profiler::Reset();
	}

	ImGui::SameLine();

	if (ImGui::Button("Clear baseline"))
		Profiler::ClearBaseline();

	ImGui::SameLine();

	if (ImGui::Button("Reset live"))
		Profiler::Reset();

	ImGui::SameLine();

	if (ImGui::Button("Copy summary"))
	{
		char summary[2048] = {};
		Profiler::BuildSummary(summary, sizeof(summary));
		ImGui::SetClipboardText(summary);
	}

	ImGui::SameLine();

	if (ImGui::Button("Export CSV"))
	{
		const std::string path = GetModLogPath("performance.csv");
		Profiler::ExportCsv(path.c_str());
	}

	ImGui::SameLine();

	if (ImGui::Button("Write to log"))
		Profiler::DumpToLog();
}

void PerformanceWindow::DrawPotatoTab()
{
	ImGui::Spacing();

	bool changed = false;
	bool potato = PotatoMode::IsActive();

	if (ImGui::Checkbox("POTATO MODE", &potato))
	{
		PotatoMode::Apply(potato);
		changed = true;
	}

	ImGui::Indent();
	Muted("%s", PotatoMode::Describe());
	Muted("The game builds its render targets once, so this takes effect after a restart or after "
		"you change any video option in the game's own menu.");
	ImGui::Unindent();

	ImGui::Spacing();

	if (ImGui::Checkbox("Draw the empty stage", &g_modVals.simpleStage))
	{
		StageColor::SetColor(0x000000u);
		StageColor::SetEnabled(g_modVals.simpleStage);
		Settings::SaveInt("Graphics", "SimpleStage", g_modVals.simpleStage ? 1 : 0);
		Settings::SaveInt("Video", "FlatStage", StageColor::IsEnabled() ? 1 : 0);
		Settings::SaveInt("Video", "FlatStageColour", 0);
		changed = true;
	}

	ImGui::Spacing();
	ImGui::SeparatorText("In force now");

	if (!RenderScale::IsInstalled() && !RenderScale::Install() && !PotatoMode::IsActive())
		Warn("%s", RenderScale::GetStatusText());

	int requestedWidth = 0;
	int requestedHeight = 0;
	RenderScale::GetRequestedSize(requestedWidth, requestedHeight);

	int liveWidth = 0;
	int liveHeight = 0;
	const bool haveLive = RenderScale::GetInForceSize(liveWidth, liveHeight);

	int observedWidth = 0;
	int observedHeight = 0;
	int observedCount = 0;
	const bool haveObserved = RenderScale::GetObservedSize(observedWidth, observedHeight,
		observedCount);

	ImGui::Text("Asked to render at %dx%d", requestedWidth, requestedHeight);

	// The engine's own size globals are the answer, not what the mod asked for and not what the
	// texture hook happened to see: the render targets are built during the game's display init,
	// which can be over before the hook that watches them is in place.
	if (haveLive && liveWidth == requestedWidth && liveHeight == requestedHeight)
	{
		ImGui::TextColored(kGoodColour, "The engine is rendering at %dx%d", liveWidth, liveHeight);
	}
	else if (haveLive)
	{
		Warn("The engine is still rendering at %dx%d. It builds its render targets once - restart "
			"the game, or change any video option in its own menu.", liveWidth, liveHeight);
	}

	if (haveObserved)
	{
		ImGui::TextDisabled("largest render target seen since the overlay loaded: %dx%d, %d of them",
			observedWidth, observedHeight, observedCount);
	}

	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	ImGui::Text("Back buffer %ux%u, multisampling %s", present.BackBufferWidth,
		present.BackBufferHeight,
		present.MultiSampleType == D3DMULTISAMPLE_NONE ? "off" : "on");

	if (!changed)
		return;

	RenderScale::Apply();
	PumpWait::Apply();
	Profiler::Reset();
}
