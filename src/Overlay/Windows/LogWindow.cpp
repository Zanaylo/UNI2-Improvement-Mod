#include "Overlay/Windows/LogWindow.h"

#include "Core/Log/LogFormat.h"
#include "Core/Log/LogText.h"
#include "Core/ShellOpen.h"
#include "Core/Text/Language.h"
#include "Core/ThreadRole.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <Windows.h>

#include <cfloat>
#include <iterator>

namespace {

constexpr DWORD kPullEveryMs = 250;
constexpr size_t kKeptEntries = 4000;
constexpr int kAllCategories = -1;
constexpr float kCategoryWidth = 170.0f;
constexpr float kSearchWidth = 220.0f;
constexpr float kTimeColumn = 90.0f;
constexpr float kLevelColumn = 64.0f;
constexpr float kThreadColumn = 56.0f;

const ImVec4 kDebugColour(0.60f, 0.60f, 0.60f, 1.0f);
const ImVec4 kWarnColour(0.95f, 0.80f, 0.35f, 1.0f);
const ImVec4 kErrorColour(0.95f, 0.45f, 0.40f, 1.0f);

uint32_t TotalCount(const LogRing& ring)
{
	uint32_t total = 0;

	for (int level = 0; level < static_cast<int>(LogLevel::Count); ++level)
		total += ring.Count(static_cast<LogLevel>(level));

	return total;
}

ImVec4 ColourOf(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Debug:
		return kDebugColour;
	case LogLevel::Warn:
		return kWarnColour;
	case LogLevel::Error:
		return kErrorColour;
	default:
		return ImGui::GetStyle().Colors[ImGuiCol_Text];
	}
}

}

LogWindow::LogWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void LogWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(860.0f), Ui::Scaled(480.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(520.0f), Ui::Scaled(260.0f)), ImVec2(FLT_MAX, FLT_MAX));
}

void LogWindow::Draw()
{
	Pull();

	if (m_dirty)
		Filter();

	DrawLoggingState();
	DrawLevels();
	DrawCategory();
	ImGui::SameLine();
	DrawSearch();
	DrawActions();
	DrawLines();
}

void LogWindow::OnClosed()
{
	std::vector<LogEntry>().swap(m_entries);
	std::vector<int>().swap(m_shown);
	std::vector<std::string>().swap(m_categories);

	m_newest = 0;
	m_total = 0;
	m_pulledAt = 0;
	m_dirty = true;
}

void LogWindow::Pull()
{
	const DWORD now = GetTickCount();

	if (m_pulledAt != 0 && now - m_pulledAt < kPullEveryMs)
		return;

	m_pulledAt = now;

	const LogRing& ring = GetLogHistory();
	const uint32_t newest = ring.Newest();
	const uint32_t total = TotalCount(ring);

	if (newest == m_newest && total == m_total)
		return;

	std::vector<LogEntry> fresh;
	ring.CopySince(m_newest > 0 ? m_newest - 1 : 0, fresh);

	m_newest = newest;
	m_total = total;
	Merge(fresh);

	if (ring.CategoryCount() != m_categories.size())
	{
		m_categories.clear();

		for (size_t i = 0; i < ring.CategoryCount(); ++i)
			m_categories.push_back(ring.CategoryName(static_cast<uint16_t>(i)));
	}

	m_dirty = true;
	m_toBottom = m_follow;
}

void LogWindow::Merge(std::vector<LogEntry>& fresh)
{
	auto first = fresh.begin();

	if (first != fresh.end() && !m_entries.empty() && first->sequence == m_entries.back().sequence)
	{
		m_entries.back() = std::move(*first);
		++first;
	}

	m_entries.insert(m_entries.end(), std::make_move_iterator(first), std::make_move_iterator(fresh.end()));

	if (m_entries.size() > kKeptEntries)
		m_entries.erase(m_entries.begin(), m_entries.begin() + (m_entries.size() - kKeptEntries));
}

void LogWindow::Filter()
{
	m_shown.clear();

	for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
	{
		if (m_entries[i].sequence > m_clearedAt && Passes(m_entries[i]))
			m_shown.push_back(i);
	}

	m_dirty = false;
}

bool LogWindow::Passes(const LogEntry& entry) const
{
	if (!m_levels[static_cast<int>(entry.level)])
		return false;

	if (m_category != kAllCategories && entry.category != m_category)
		return false;

	return m_search.PassFilter(entry.text.c_str());
}

void LogWindow::DrawLoggingState()
{
	if (IsLoggerOpen())
	{
		UiText::Muted("Also written to %s", GetLogFilePath().c_str());
		return;
	}

	UiText::Warn("The log file is off, so only warnings and errors are kept here.");

	if (ImGui::SmallButton(TrLabel("Turn it on")))
		SetLogging(true);
}

void LogWindow::DrawLevels()
{
	DrawLevel(LogLevel::Debug, Tr("Debug"));
	ImGui::SameLine();
	DrawLevel(LogLevel::Info, Tr("Info"));
	ImGui::SameLine();
	DrawLevel(LogLevel::Warn, Tr("Warning"));
	ImGui::SameLine();
	DrawLevel(LogLevel::Error, Tr("Error"));
}

void LogWindow::DrawLevel(LogLevel level, const char* name)
{
	const int index = static_cast<int>(level);
	const std::string label = std::string(name) + " (" + std::to_string(GetLogHistory().Count(level)) +
		")###level" + std::to_string(index);

	ImGui::PushStyleColor(ImGuiCol_Text, ColourOf(level));

	if (ImGui::Checkbox(label.c_str(), &m_levels[index]))
		m_dirty = true;

	ImGui::PopStyleColor();
}

void LogWindow::DrawCategory()
{
	const int count = static_cast<int>(m_categories.size());
	const char* const preview = m_category == kAllCategories || m_category >= count
		? Tr("Every category")
		: m_categories[m_category].c_str();

	const int current = m_category + 1;
	int picked = current;

	Ui::SetItemWidth(kCategoryWidth);

	if (ImGui::BeginCombo("##logcategory", preview))
	{
		if (ImGui::Selectable(Tr("Every category"), current == 0))
			picked = 0;

		ComboNav::KeepSelectedInView(current == 0);

		for (int i = 0; i < count; ++i)
		{
			const bool selected = current == i + 1;

			if (ImGui::Selectable(m_categories[i].c_str(), selected))
				picked = i + 1;

			ComboNav::KeepSelectedInView(selected);
		}

		ImGui::EndCombo();
	}

	if (picked == current)
		picked = ComboNav::WheelTarget(current, count + 1);

	if (picked == current)
		return;

	m_category = picked - 1;
	m_dirty = true;
}

void LogWindow::DrawSearch()
{
	Ui::SetItemWidth(kSearchWidth);

	if (ImGui::InputTextWithHint("##logsearch", Tr("Search the log"), m_search.InputBuf,
		IM_ARRAYSIZE(m_search.InputBuf)))
	{
		m_search.Build();
		m_dirty = true;
	}
}

void LogWindow::DrawActions()
{
	ImGui::Checkbox(TrLabel("Follow new lines"), &m_follow);
	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Copy")))
		CopyShown();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", Tr("Copies the lines shown, to paste in a bug report."));

	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Clear")))
	{
		m_clearedAt = m_newest;
		m_dirty = true;
	}

	ImGui::SameLine();

	const std::string file = GetLogFilePath();

	ImGui::BeginDisabled(file.empty());

	if (ImGui::Button(TrLabel("Open the log file")))
		ShellOpen::Open(file);

	ImGui::EndDisabled();
	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Open the folder")))
		ShellOpen::Open(GetModLogPath());

	UiText::Muted("%d of %d line(s)", static_cast<int>(m_shown.size()), static_cast<int>(m_entries.size()));
}

void LogWindow::DrawLines()
{
	const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;

	if (!ImGui::BeginTable("##loglines", 4, flags, ImGui::GetContentRegionAvail()))
		return;

	ImGui::TableSetupColumn(Tr("Time"), ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kTimeColumn));
	ImGui::TableSetupColumn(Tr("Level"), ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kLevelColumn));
	ImGui::TableSetupColumn(Tr("Thread"), ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kThreadColumn));
	ImGui::TableSetupColumn(Tr("Message"), ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(m_shown.size()));

	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			DrawLine(m_entries[m_shown[row]]);
	}

	if (m_toBottom)
	{
		ImGui::SetScrollHereY(1.0f);
		m_toBottom = false;
	}

	ImGui::EndTable();
}

void LogWindow::DrawLine(const LogEntry& entry)
{
	ImGui::PushID(static_cast<int>(entry.sequence));
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::TextDisabled("%s", LogFormat::Time(entry.msOfDay).c_str());

	ImGui::TableNextColumn();
	ImGui::TextColored(ColourOf(entry.level), "%s", Tr(LogText::LevelName(entry.level)));

	ImGui::TableNextColumn();
	ImGui::TextDisabled("%s", ThreadRole::Tag(static_cast<ThreadRole::Role>(entry.thread)));

	ImGui::TableNextColumn();
	ImGui::PushStyleColor(ImGuiCol_Text, ColourOf(entry.level));
	ImGui::Selectable(entry.text.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
	ImGui::PopStyleColor();

	if (ImGui::BeginPopupContextItem("##logline"))
	{
		if (ImGui::MenuItem(TrLabel("Copy this line")))
			ImGui::SetClipboardText(LogFormat::Line(entry).c_str());

		ImGui::EndPopup();
	}

	if (entry.repeats > 0)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("x%u", entry.repeats + 1);
	}

	ImGui::PopID();
}

void LogWindow::CopyShown() const
{
	std::vector<LogEntry> shown;
	shown.reserve(m_shown.size());

	for (const int index : m_shown)
		shown.push_back(m_entries[index]);

	ImGui::SetClipboardText(LogFormat::Lines(shown).c_str());
}
