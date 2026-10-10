#include "Overlay/Native/OptionScreen.h"

#include "Core/ThreadRole.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "Game/Engine/GameOffsets.h"
#include "Overlay/Native/NativeDraw.h"

namespace {

using NativeDraw::Frame;

constexpr float kBandTop = 135.0f;
constexpr float kBandHeight = 32.0f;
constexpr float kBodyBottom = 535.0f;

constexpr float kFirstRowY = 216.0f;
constexpr float kRowPitch = 32.0f;
constexpr float kActionGap = 8.0f;
constexpr float kSelectLeft = 160.0f;
constexpr float kSelectRight = 1120.0f;
constexpr float kSelectHeight = 30.0f;
constexpr float kNameX = 239.0f;
constexpr float kValueCentreX = 959.0f;
constexpr float kArrowOffset = 110.0f;
constexpr float kRowPixels = 20.0f;

constexpr NativeDraw::InfoBox kInfoBox = { 535.0f, 610.0f };

constexpr float kPromptRight = 1215.0f;
constexpr float kPromptGap = 6.0f;
constexpr float kPromptGroupGap = 24.0f;

constexpr int kWordChange = 10;
constexpr int kWordReturn = 130;

constexpr uint32_t kDim = 0x80000000;
constexpr uint32_t kBody = 0xD83A4047;
constexpr uint32_t kBandLight = 0xFFE6E6E6;
constexpr uint32_t kBandShade = 0xFFBCBCBC;
constexpr uint32_t kBandEdge = 0xFF7C7C7C;
constexpr uint32_t kText = 0xFFFFFFFF;
constexpr uint32_t kChanged = 0xFF64FFFF;
constexpr uint32_t kSelectStrong = 0xF0D40000;
constexpr uint32_t kSelectWeak = 0xE0880000;
constexpr uint32_t kSelectEdge = 0xC0FF6060;

const char* const kConfirmWord = "Confirm";
const char* const kResetWord = "Reset to default";
const char* const kFallbackSelect = "Select";
const char* const kFallbackChange = "Change";
const char* const kFallbackReturn = "Return";

const char* const kConfirmInfo = "Keep the settings above.";
const char* const kResetInfo = "Put every setting back to its default.";
const char* const kReturnInfo = "Leave and undo the changes made here.";

float RowY(const OptionScreenState& state, int row)
{
	const float gap = row >= state.Settings() ? kActionGap : 0.0f;

	return kFirstRowY + row * kRowPitch + gap;
}

void DrawFrame(const Frame& frame)
{
	QuadRenderer::FillRect(0.0f, 0.0f, frame.width, frame.height, kDim);
	QuadRenderer::FillRect(0.0f, frame.Y(kBandTop), frame.width, frame.S(kBodyBottom - kBandTop), kBody);
	QuadRenderer::FillRectVertical(0.0f, frame.Y(kBandTop), frame.width, frame.S(kBandHeight), kBandLight,
		kBandShade);
	QuadRenderer::FillRect(0.0f, frame.Y(kBandTop + kBandHeight - 1.0f), frame.width, frame.S(1.0f), kBandEdge);
}

void DrawSelection(const Frame& frame, float centreY)
{
	const float top = centreY - kSelectHeight * 0.5f;
	const float width = frame.S(kSelectRight - kSelectLeft);

	QuadRenderer::FillRectHorizontal(frame.X(kSelectLeft), frame.Y(top), width * 0.5f, frame.S(kSelectHeight),
		kSelectWeak, kSelectStrong);
	QuadRenderer::FillRectHorizontal(frame.X(kSelectLeft) + width * 0.5f, frame.Y(top), width * 0.5f,
		frame.S(kSelectHeight), kSelectStrong, kSelectWeak);
	QuadRenderer::StrokeRect(frame.X(kSelectLeft), frame.Y(top), width, frame.S(kSelectHeight), frame.S(1.0f),
		kSelectEdge);
}

void DrawSetting(const Frame& frame, const OptionScreenState& state, int row, float centreY)
{
	const OptionMenu::RowSpec spec = state.Client().Row(row);
	const int value = state.Value(row);
	const char* const text = value >= 0 && value < spec.choiceCount ? spec.choices[value] : "";
	const float width = NativeDraw::Width(frame, text, kRowPixels);

	NativeDraw::Text(frame, spec.word, kNameX, centreY, kRowPixels, kText);
	NativeDraw::Text(frame, text, kValueCentreX - width * 0.5f, centreY, kRowPixels,
		state.IsChanged(row) ? kChanged : kText);

	if (value > 0)
		NativeDraw::Text(frame, "<", kValueCentreX - kArrowOffset, centreY, kRowPixels, kText);

	if (value < spec.choiceCount - 1)
		NativeDraw::Text(frame, ">", kValueCentreX + kArrowOffset, centreY, kRowPixels, kText);
}

const char* ActionWord(int action, char* out)
{
	if (action == OptionScreenState::kConfirmRow)
		return kConfirmWord;

	if (action == OptionScreenState::kResetRow)
		return kResetWord;

	NativeDraw::Word(kWordReturn, kFallbackReturn, out);
	return out;
}

void DrawRows(const Frame& frame, const OptionScreenState& state)
{
	DrawSelection(frame, RowY(state, state.Cursor()));

	for (int row = 0; row < state.Settings(); ++row)
		DrawSetting(frame, state, row, RowY(state, row));

	for (int action = 0; action < OptionScreenState::kActions; ++action)
	{
		char word[NativeDraw::kWordBytes] = {};
		NativeDraw::Text(frame, ActionWord(action, word), kNameX, RowY(state, state.Settings() + action),
			kRowPixels, kText);
	}
}

const char* Information(const OptionScreenState& state)
{
	const int cursor = state.Cursor();
	const int action = cursor - state.Settings();

	if (action < 0)
		return state.Client().Row(cursor).info;

	if (action == OptionScreenState::kConfirmRow)
		return kConfirmInfo;

	return action == OptionScreenState::kResetRow ? kResetInfo : kReturnInfo;
}

void DrawPrompt(const Frame& frame)
{
	NativeDraw::PromptBar(frame);

	char select[NativeDraw::kWordBytes] = {};
	char change[NativeDraw::kWordBytes] = {};
	char back[NativeDraw::kWordBytes] = {};
	NativeDraw::Word(GameOffsets::kMenuWordSelect, kFallbackSelect, select);
	NativeDraw::Word(kWordChange, kFallbackChange, change);
	NativeDraw::Word(kWordReturn, kFallbackReturn, back);

	static const int kCancel[] = { GameOffsets::kMenuKeyCancel };
	static const int kConfirm[] = { GameOffsets::kMenuKeyConfirm };
	static const int kSideways[] = { GameOffsets::kMenuKeyLeft, GameOffsets::kMenuKeyRight };
	static const int kUpDown[] = { GameOffsets::kMenuKeyUp, GameOffsets::kMenuKeyDown };

	float right = kPromptRight;

	right -= NativeDraw::PromptText(frame, back, right) + kPromptGap;
	right -= NativeDraw::PromptKeys(frame, kCancel, 1, right) + kPromptGroupGap;
	right -= NativeDraw::PromptText(frame, kConfirmWord, right) + kPromptGap;
	right -= NativeDraw::PromptKeys(frame, kConfirm, 1, right) + kPromptGroupGap;
	right -= NativeDraw::PromptText(frame, change, right) + kPromptGap;
	right -= NativeDraw::PromptKeys(frame, kSideways, 2, right) + kPromptGroupGap;
	right -= NativeDraw::PromptText(frame, select, right) + kPromptGap;
	NativeDraw::PromptKeys(frame, kUpDown, 2, right);
}

}

OptionScreen::OptionScreen(OptionMenu::IClient& client)
	: m_state(client)
	, m_open(false)
{
}

void OptionScreen::Open()
{
	m_state.Open();
	m_open.store(true);
}

bool OptionScreen::IsOpen() const
{
	return m_open.load();
}

void OptionScreen::Close()
{
	m_open.store(false);
}

void OptionScreen::Update(const MenuInput::State& input)
{
	if (m_state.Handle(input) == OptionScreenState::Action_Close)
		Close();
}

void OptionScreen::Render(IDirect3DDevice9* device) const
{
	EXPECT_THREAD(ThreadRole::Role_Render);

	Frame frame = {};

	if (!m_open.load() || !TrainingMenu::IsActive() || !NativeDraw::Begin(device, frame))
		return;

	DrawFrame(frame);
	NativeDraw::Title(frame, m_state.Client().Title(), kBandTop + kBandHeight * 0.5f, GameArt::kGearIcon);
	DrawRows(frame, m_state);
	NativeDraw::Information(frame, Information(m_state), kInfoBox);
	DrawPrompt(frame);
}
