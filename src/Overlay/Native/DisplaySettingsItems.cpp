#include "Overlay/Native/DisplaySettingsItems.h"

#include "Core/CrossThread.h"
#include "Core/ThreadRole.h"
#include "D3D9/Post/PostOptions.h"
#include "D3D9/Post/PostStages.h"
#include "D3D9/Post/UpscaleFilter.h"
#include "Game/Display/Improvements.h"
#include "Game/Display/PotatoMode.h"
#include "Game/Menus/OptionMenu.h"

#include <atomic>
#include <cstdio>

namespace {

constexpr int kNoRequest = -1;
constexpr int kMaxChoices = 8;
constexpr int kHeightCount = static_cast<int>(sizeof(PotatoMode::kHeights) / sizeof(PotatoMode::kHeights[0]));
constexpr int kDefaultHeight = 1;

const char* const kTitle = "IMPROVEMENT MOD - DISPLAY SETTINGS";

char g_heightNames[kHeightCount][16] = {};

int ReadHeight()
{
	const int height = PotatoMode::GetHeight();

	for (int i = 0; i < kHeightCount; ++i)
	{
		if (PotatoMode::kHeights[i] == height)
			return i;
	}

	return 0;
}

void WriteHeight(int index)
{
	PotatoMode::SetHeight(PotatoMode::kHeights[index]);
}

struct Setting
{
	const char* word;
	const char* info;
	int count;
	const char* (*name)(int);
	int defaultValue;
	int (*read)();
	void (*write)(int);
};

const char* HeightName(int index)
{
	return g_heightNames[index];
}

const Setting kSettings[] = {
	{ "Improvements", "Draw the frame bigger than the window and scale it down. Needs a restart.",
		Improvements::Level_COUNT, &Improvements::GetLevelName, Improvements::Level_Off,
		&Improvements::GetLevel, &Improvements::Apply },
	{ "POTATO MODE", "Draw less to keep 60 fps on a weak PC. Gameplay does not change.",
		PotatoMode::Level_COUNT, &PotatoMode::GetLevelName, PotatoMode::Level_Off,
		&PotatoMode::GetLevel, &PotatoMode::Apply },
	{ "Potato resolution", "The size the frame is drawn at on the Potato level.",
		kHeightCount, &HeightName, kDefaultHeight, &ReadHeight, &WriteHeight },
	{ "Upscale filter", "The filter that stretches the 1280x720 scene to your window.",
		UpscaleFilter::Kind_COUNT, &UpscaleFilter::GetName, UpscaleFilter::Kind_Off,
		&PostStages::GetUpscaleFilter, &PostStages::SetUpscaleFilter },
	{ "Anti-aliasing", "FXAA over the finished frame.",
		AntiAlias::Level_COUNT, &AntiAlias::GetName, AntiAlias::Level_Off,
		&PostStages::GetAntiAliasing, &PostStages::SetAntiAliasing },
	{ "Sharpening", "Brings back the edges the stretch to your window softened.",
		SharpenMode::Kind_COUNT, &SharpenMode::GetName, SharpenMode::Kind_Off,
		&PostStages::GetSharpening, &PostStages::SetSharpening },
};

constexpr int kSettingCount = static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));

const char* g_choices[kSettingCount][kMaxChoices] = {};

std::atomic<int> g_current[kSettingCount];
RequestSlot<int, kNoRequest> g_requested[kSettingCount];

void BuildChoices()
{
	for (int i = 0; i < kHeightCount; ++i)
		sprintf_s(g_heightNames[i], "%dp", PotatoMode::kHeights[i]);

	for (int row = 0; row < kSettingCount; ++row)
	{
		const Setting& setting = kSettings[row];

		for (int choice = 0; choice < setting.count && choice < kMaxChoices; ++choice)
			g_choices[row][choice] = setting.name(choice);
	}
}

class Client : public OptionMenu::IClient
{
public:
	const char* EntryWord() const override
	{
		return "Improvement Mod - Display Settings";
	}

	const char* EntryInfo() const override
	{
		return "Present size, POTATO MODE and the shaders of UNI2 Improvement Mod.";
	}

	const char* Title() const override
	{
		return kTitle;
	}

	int RowCount() const override
	{
		return kSettingCount;
	}

	OptionMenu::RowSpec Row(int index) const override
	{
		const Setting& setting = kSettings[index];

		return { setting.word, setting.info, g_choices[index], setting.count };
	}

	int Current(int index) const override
	{
		return g_current[index].load();
	}

	int Default(int index) const override
	{
		return kSettings[index].defaultValue;
	}

	void Apply(int index, int value) override
	{
		if (value == g_current[index].load())
			return;

		g_requested[index].Post(value);
		g_current[index].store(value);
	}
};

Client g_client;

}

bool DisplaySettingsItems::Install()
{
	BuildChoices();

	for (int i = 0; i < kSettingCount; ++i)
	{
		g_current[i].store(kSettings[i].read());
		g_requested[i].Clear();
	}

	return OptionMenu::Install(&g_client);
}

void DisplaySettingsItems::OnFrame()
{
	EXPECT_THREAD(ThreadRole::Role_Render);

	for (int i = 0; i < kSettingCount; ++i)
	{
		int requested = kNoRequest;

		if (g_requested[i].Take(requested))
			kSettings[i].write(requested);

		g_current[i].store(kSettings[i].read());
	}
}
