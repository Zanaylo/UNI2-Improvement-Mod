#pragma once

#include "Core/Log/LogRing.h"
#include "Overlay/Framework/IWindow.h"

#include <imgui.h>

#include <string>
#include <vector>

class LogWindow : public IWindow
{
public:
	LogWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;
	void OnClosed() override;

private:
	void Pull();
	void Merge(std::vector<LogEntry>& fresh);
	void Filter();
	bool Passes(const LogEntry& entry) const;

	void DrawLoggingState();
	void DrawLevels();
	void DrawLevel(LogLevel level, const char* name);
	void DrawCategory();
	void DrawSearch();
	void DrawActions();
	void DrawLines();
	void DrawLine(const LogEntry& entry);
	void CopyShown() const;

	std::vector<LogEntry> m_entries;
	std::vector<int> m_shown;
	std::vector<std::string> m_categories;
	ImGuiTextFilter m_search;
	uint32_t m_newest = 0;
	uint32_t m_total = 0;
	uint32_t m_clearedAt = 0;
	unsigned long m_pulledAt = 0;
	int m_category = -1;
	bool m_levels[static_cast<int>(LogLevel::Count)] = { false, true, true, true };
	bool m_follow = true;
	bool m_toBottom = false;
	bool m_dirty = true;
};
