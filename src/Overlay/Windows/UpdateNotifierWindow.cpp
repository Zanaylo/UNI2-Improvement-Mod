#include "Overlay/Windows/UpdateNotifierWindow.h"

#include "Core/ShellOpen.h"
#include "Core/info.h"
#include "Overlay/Widgets/BoldText.h"
#include "Overlay/Widgets/MarkdownView.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"
#include "Web/UpdateCheck.h"
#include "Web/UpdateInstall.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace {

constexpr float kWidth = 720.0f;
constexpr float kHeight = 560.0f;
constexpr float kDisplayShare = 0.9f;
constexpr float kButtonWidth = 170.0f;
constexpr int kButtonCount = 3;
constexpr float kHeadlineScale = 1.25f;
constexpr float kReleaseTitleScale = 1.18f;
constexpr int kMonths = 12;

const ImVec4 kHeadlineColour(0.95f, 0.95f, 1.00f, 1.00f);
const ImVec4 kTitleColour(0.98f, 0.98f, 1.00f, 1.00f);
const ImVec4 kTitleHoverColour(0.76f, 0.76f, 0.80f, 1.00f);

const char* const kMonthNames[kMonths] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

const char* const kInstallHelp = "Closes the game and installs the update. Steam starts the game again "
	"when it is done.";
const char* const kManualHelp = "To install by hand, close the game first. The mod files are locked "
	"while it runs.";

std::string FormattedDate(const std::string& published)
{
	int year = 0;
	int month = 0;
	int day = 0;

	if (sscanf_s(published.c_str(), "%d-%d-%d", &year, &month, &day) != 3 || month < 1 || month > kMonths)
		return std::string();

	char text[32] = {};
	sprintf_s(text, "%s %d, %d", kMonthNames[month - 1], day, year);
	return text;
}

void Centre(float width)
{
	const float free = ImGui::GetContentRegionAvail().x - width;

	if (free > 0.0f)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + free * 0.5f);
}

void CentredText(const char* text)
{
	Centre(ImGui::CalcTextSize(text).x);
	ImGui::TextUnformatted(text);
}

void CentredMuted(const char* text)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	CentredText(text);
	ImGui::PopStyleColor();
}

void DrawTitleLink(const std::string& title, const std::string& page)
{
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kReleaseTitleScale);

	const ImVec2 size = BoldText::Size(title.c_str());
	Centre(size.x);

	const ImVec2 at = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##release", size);

	const bool hovered = ImGui::IsItemHovered();
	const ImU32 colour = ImGui::GetColorU32(hovered ? kTitleHoverColour : kTitleColour);
	ImDrawList* const draw = ImGui::GetWindowDrawList();

	BoldText::Draw(draw, at, colour, title.c_str());

	if (hovered)
	{
		draw->AddLine(ImVec2(at.x, at.y + size.y), ImVec2(at.x + size.x, at.y + size.y), colour);
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}

	ImGui::PopFont();

	if (hovered)
		ImGui::SetTooltip("Open this release on GitHub");

	if (ImGui::IsItemClicked() && !page.empty())
		ShellOpen::Open(page);
}

}

UpdateNotifierWindow::UpdateNotifierWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void UpdateNotifierWindow::BeforeDraw()
{
	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 wanted = Ui::Scaled(kWidth, kHeight);
	const ImVec2 least((std::min)(wanted.x, io.DisplaySize.x * kDisplayShare),
		(std::min)(wanted.y, io.DisplaySize.y * kDisplayShare));

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
		ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(wanted, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(least, ImVec2(FLT_MAX, FLT_MAX));
}

void UpdateNotifierWindow::Refresh()
{
	if (ImGui::IsWindowAppearing())
		m_canInstall = UpdateInstall::IsAvailable();

	const long revision = UpdateCheck::Revision();

	if (revision == m_revision)
		return;

	m_revision = revision;
	m_releases.clear();

	for (const GitHubRelease::Release& release : UpdateCheck::GetNewerReleases())
	{
		m_releases.push_back({ release.tag, release.title.empty() ? release.tag : release.title, release.page,
			FormattedDate(release.published), Markdown::Parse(release.notes) });
	}

	m_canInstall = UpdateInstall::IsAvailable();
	m_scrollToTop = true;
}

void UpdateNotifierWindow::Draw()
{
	Refresh();
	DrawHeader();
	ImGui::Separator();
	DrawNotes();
	DrawButtons();
}

void UpdateNotifierWindow::DrawHeader() const
{
	char line[128] = {};

	sprintf_s(line, "%s %s is available", UNI2_IM_NAME, UpdateCheck::GetLatestVersion());
	ImGui::PushStyleColor(ImGuiCol_Text, kHeadlineColour);
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kHeadlineScale);
	CentredText(line);
	ImGui::PopFont();
	ImGui::PopStyleColor();

	const int count = static_cast<int>(m_releases.size());

	if (count > 1)
		sprintf_s(line, "You have %s, %d releases behind", UNI2_IM_VERSION, count);
	else
		sprintf_s(line, "You have %s", UNI2_IM_VERSION);

	CentredMuted(line);
	ImGui::Spacing();
}

float UpdateNotifierWindow::BottomReserve() const
{
	const ImGuiStyle& style = ImGui::GetStyle();
	const bool failed = UpdateInstall::GetError()[0] != '\0';
	const int textLines = failed ? 2 : 1;

	return ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() * textLines +
		style.ItemSpacing.y * 2.0f;
}

void UpdateNotifierWindow::DrawNotes()
{
	UiText::Muted("Release notes");

	ImGui::BeginChild("##notes", ImVec2(0.0f, -BottomReserve()), ImGuiChildFlags_Borders);

	if (m_scrollToTop)
	{
		ImGui::SetScrollY(0.0f);
		m_scrollToTop = false;
	}

	if (m_releases.empty())
		UiText::Muted("This release came without notes.");

	for (size_t i = 0; i < m_releases.size(); ++i)
	{
		if (i > 0)
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
		}

		ImGui::PushID(static_cast<int>(i));
		DrawRelease(m_releases[i]);
		ImGui::PopID();
	}

	ImGui::EndChild();
}

void UpdateNotifierWindow::DrawRelease(const ReleaseNotes& release) const
{
	CentredMuted(release.tag.c_str());
	DrawTitleLink(release.title, release.page);

	if (!release.date.empty())
		CentredMuted(release.date.c_str());

	ImGui::Spacing();

	if (release.notes.empty())
	{
		UiText::Muted("No notes for this release.");
		return;
	}

	MarkdownView::Draw(release.notes);
}

void UpdateNotifierWindow::DrawPrimaryButton(const ImVec2& size)
{
	if (!m_canInstall)
	{
		if (ImGui::Button("Open the release page", size))
			ShellOpen::Open(UpdateCheck::GetReleaseUrl());

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Opens the release on GitHub in your browser.");

		return;
	}

	if (ImGui::Button("Update", size))
		UpdateInstall::Start();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", kInstallHelp);
}

void UpdateNotifierWindow::DrawButtons()
{
	const char* const error = UpdateInstall::GetError();

	if (error[0] != '\0')
		UiText::Warn("%s", error);

	ImGui::Spacing();

	const ImVec2 size(Ui::Scaled(kButtonWidth), 0.0f);
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	Centre(size.x * kButtonCount + spacing * (kButtonCount - 1));

	DrawPrimaryButton(size);
	ImGui::SameLine();

	if (ImGui::Button("Later", size))
	{
		UpdateCheck::Dismiss();
		Close();
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Asks again the next time the game starts.");

	ImGui::SameLine();

	if (ImGui::Button("Skip this version", size))
	{
		UpdateCheck::SkipLatest();
		Close();
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Stops asking about %s. A newer release will still be announced.",
			UpdateCheck::GetLatestVersion());

	CentredMuted(kManualHelp);
}
