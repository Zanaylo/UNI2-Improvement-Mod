#include "Overlay/Native/TrainingMenuItems.h"

#include "Core/CrossThread.h"
#include "Core/Harness/CleanFrame.h"
#include "Core/ThreadRole.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Menus/TrainingMenu.h"
#include "Overlay/Guides/FrameMeterLegend.h"
#include "Overlay/Guides/HitboxLegend.h"
#include "Overlay/Hud/FrameMeterHud.h"
#include "Overlay/Hud/GrdPopupHud.h"
#include "Overlay/Hud/HealthReadout.h"
#include "Overlay/Hud/ProrationHud.h"
#include "Overlay/Native/GuideScreen.h"
#include "Overlay/Native/HudOpacityItems.h"
#include "Overlay/Native/OptionScreen.h"
#include "Overlay/Windows/HitboxOverlay.h"

#include <atomic>

namespace {

constexpr int kNoRequest = TrainingMenu::kNoValue;

const char* const kShowChoices[] = { "<GR_NM_NotShown>", "<GR_NM_Show>" };
constexpr int kShowChoiceCount = static_cast<int>(sizeof(kShowChoices) / sizeof(kShowChoices[0]));

const TrainingMenu::PageSpec kPage = { "Improvement Mod", "Settings added by UNI2 Improvement Mod." };

GuideScreen g_frameMeterGuide(&FrameMeterLegend::Get);
GuideScreen g_hitboxGuide(&HitboxLegend::Get);
OptionScreen g_hudOpacityScreen(HudOpacityItems::Client());

const TrainingMenu::ItemSpec kHudOpacityItem = { 0x1008, "HUD Opacity",
	"How see-through the menus, the HUD and the Training displays are. Confirm to change them.",
	nullptr, 0, 0, true };

struct Binding
{
	int id;
	const char* word;
	const char* info;
	bool (*read)();
	void (*write)(bool);
	GuideScreen* guide;
};

const Binding kBindings[] = {
	{ 0x1001, "Frame Meter Display",
		"Toggle the mod's frame meter. Open menu shows what every colour and number means.",
		&FrameMeterHud::IsVisible, &FrameMeterHud::SetVisible, &g_frameMeterGuide },
	{ 0x1002, "Hitbox Display",
		"Toggle the mod's hitbox viewer. Open menu shows what every box means.",
		&HitboxOverlay::IsShown, &HitboxOverlay::SetShown, &g_hitboxGuide },
	{ 0x1003, "GRD Popups",
		"Show each GRD gain or loss, in blocks, above the GRD gauge.",
		&GrdPopupHud::IsVisible, &GrdPopupHud::SetVisible, nullptr },
	{ 0x1004, "GRD Timer",
		"Show the seconds left before the GRD circle closes, in the middle of the gauge.",
		&GrdPopupHud::IsTimerVisible, &GrdPopupHud::SetTimerVisible, nullptr },
	{ 0x1005, "Health Values",
		"Show the exact health under each health bar.",
		&HealthReadout::IsVisible, &HealthReadout::SetVisible, nullptr },
	{ 0x1006, "Proration Display",
		"Show the proration the combo timer and the move count put on the next hit, under Damage info.",
		&ProrationHud::IsVisible, &ProrationHud::SetVisible, nullptr },
	{ 0x1007, "Characters and Effects",
		"Hide the characters, their shadows and the effects, so only the stage is drawn.",
		[]() { return !CleanFrame::FightersHidden(); },
		[](bool shown) { CleanFrame::SetFightersHidden(!shown); }, nullptr },
};

constexpr int kBindingCount = static_cast<int>(sizeof(kBindings) / sizeof(kBindings[0]));

std::atomic<int> g_shown[kBindingCount];
RequestSlot<int, kNoRequest> g_requested[kBindingCount];

class Client : public TrainingMenu::IClient
{
public:
	TrainingMenu::PageSpec Page() const override
	{
		return kPage;
	}

	int ItemCount() const override
	{
		return OnlineState::IsOnline() ? 0 : kBindingCount + 1;
	}

	TrainingMenu::ItemSpec Item(int index) const override
	{
		if (index >= kBindingCount)
			return kHudOpacityItem;

		const Binding& binding = kBindings[index];

		return { binding.id, binding.word, binding.info, kShowChoices, kShowChoiceCount,
			g_shown[index].load() };
	}

	void BeforeUpdate() override
	{
		for (int i = 0; i < kBindingCount; ++i)
		{
			if (!g_requested[i].IsPending())
				TrainingMenu::SetValue(kBindings[i].id, g_shown[i].load());
		}
	}

	void AfterUpdate() override
	{
		for (int i = 0; i < kBindingCount; ++i)
		{
			const int value = TrainingMenu::GetValue(kBindings[i].id);

			if (value == TrainingMenu::kNoValue || value == g_shown[i].load())
				continue;

			g_requested[i].Post(value);
			g_shown[i].store(value);
		}
	}

	bool OnOpenPicker(int id) override
	{
		for (const Binding& binding : kBindings)
		{
			if (binding.id == id)
				return Open(binding.guide);
		}

		return false;
	}

	bool OnConfirm(int id) override
	{
		return id == kHudOpacityItem.id && Open(&g_hudOpacityScreen);
	}

private:
	template <typename Screen>
	static bool Open(Screen* screen)
	{
		if (screen == nullptr)
			return true;

		screen->Open();
		TrainingMenu::OpenModal(screen);
		return true;
	}
};

Client g_client;

}

bool TrainingMenuItems::Install()
{
	for (int i = 0; i < kBindingCount; ++i)
	{
		g_shown[i].store(0);
		g_requested[i].Clear();
	}

	return TrainingMenu::Install(&g_client);
}

void TrainingMenuItems::OnFrame()
{
	EXPECT_THREAD(ThreadRole::Role_Render);

	for (int i = 0; i < kBindingCount; ++i)
	{
		int requested = kNoRequest;

		if (g_requested[i].Take(requested))
			kBindings[i].write(requested != 0);

		g_shown[i].store(kBindings[i].read() ? 1 : 0);
	}
}

void TrainingMenuItems::Render(IDirect3DDevice9* device)
{
	g_hudOpacityScreen.Render(device);

	for (const Binding& binding : kBindings)
	{
		if (binding.guide != nullptr)
			binding.guide->Render(device);
	}
}
