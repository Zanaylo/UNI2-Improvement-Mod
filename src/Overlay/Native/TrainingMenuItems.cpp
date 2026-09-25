#include "Overlay/Native/TrainingMenuItems.h"

#include "Core/CrossThread.h"
#include "Core/ThreadRole.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Menus/TrainingMenu.h"
#include "Overlay/Guides/FrameMeterLegend.h"
#include "Overlay/Guides/HitboxLegend.h"
#include "Overlay/Hud/FrameMeterHud.h"
#include "Overlay/Native/GuideScreen.h"
#include "Overlay/Windows/HitboxOverlay.h"

#include <atomic>

namespace {

constexpr int kNoRequest = TrainingMenu::kNoValue;

const char* const kShowChoices[] = { "<GR_NM_NotShown>", "<GR_NM_Show>" };
constexpr int kShowChoiceCount = static_cast<int>(sizeof(kShowChoices) / sizeof(kShowChoices[0]));

const TrainingMenu::PageSpec kPage = { "Improvement Mod", "Settings added by UNI2 Improvement Mod." };

GuideScreen g_frameMeterGuide(&FrameMeterLegend::Get);
GuideScreen g_hitboxGuide(&HitboxLegend::Get);

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
		return OnlineState::IsOnline() ? 0 : kBindingCount;
	}

	TrainingMenu::ItemSpec Item(int index) const override
	{
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
			if (binding.id != id)
				continue;

			binding.guide->Open();
			TrainingMenu::OpenModal(binding.guide);
			return true;
		}

		return false;
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
	for (const Binding& binding : kBindings)
		binding.guide->Render(device);
}
