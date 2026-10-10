#include "Overlay/Native/OptionTitle.h"

#include "Core/ThreadRole.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "Game/Menus/OptionMenu.h"
#include "Overlay/Native/NativeDraw.h"

namespace {

constexpr float kBarHeight = 32.0f;
constexpr float kBarInset = 2.0f;
constexpr float kNarrowest = 256.0f;
constexpr float kPadding = 24.0f;
constexpr float kFeather = 64.0f;
constexpr uint32_t kBar = 0xFFE6E6E6;
constexpr uint32_t kBarClear = 0x00E6E6E6;

}

void OptionTitle::Render(IDirect3DDevice9* device)
{
	EXPECT_THREAD(ThreadRole::Role_Render);

	OptionMenu::TitleView view = {};
	NativeDraw::Frame frame = {};

	if (!OptionMenu::ShownTitle(view) || !NativeDraw::Begin(device, frame))
		return;

	const float textWidth = NativeDraw::TitleWidth(frame, view.text, GameArt::kGearIcon) + kPadding * 2.0f;
	const float width = textWidth > kNarrowest ? textWidth : kNarrowest;
	const float top = static_cast<float>(view.top);

	const float left = NativeDraw::kCentre - width * 0.5f;
	const float barTop = frame.Y(top + kBarInset);
	const float barHeight = frame.S(kBarHeight - kBarInset * 2.0f);

	QuadRenderer::FillRectHorizontal(frame.X(left - kFeather), barTop, frame.S(kFeather), barHeight, kBarClear, kBar);
	QuadRenderer::FillRect(frame.X(left), barTop, frame.S(width), barHeight, kBar);
	QuadRenderer::FillRectHorizontal(frame.X(left + width), barTop, frame.S(kFeather), barHeight, kBar, kBarClear);
	NativeDraw::Title(frame, view.text, top + kBarHeight * 0.5f, GameArt::kGearIcon);
}
