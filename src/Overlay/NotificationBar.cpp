#include "Overlay/NotificationBar.h"

#include "Core/interfaces.h"

#include <imgui.h>

#include <cstdarg>
#include <cstdio>
#include <deque>
#include <string>

namespace {

constexpr float kSecondsToCross = 9.0f;
constexpr int kMaxPending = 8;

std::deque<std::string> g_pending;
bool g_measured = false;
float g_offset = 0.0f;
float g_textWidth = 0.0f;
float g_screenWidth = 0.0f;

void Measure(const std::string& text)
{
	g_textWidth = ImGui::CalcTextSize(text.c_str()).x;
	g_offset = g_screenWidth;
	g_measured = true;
}

}

void NotificationBar::Add(const char* format, ...)
{
	if (format == nullptr || !g_modVals.notifications)
		return;

	va_list args;
	va_start(args, format);
	char text[512] = {};
	const int written = vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	if (written <= 0)
		return;

	if (static_cast<int>(g_pending.size()) >= kMaxPending)
		g_pending.pop_front();

	g_pending.emplace_back(text);
}

void NotificationBar::Clear()
{
	g_pending.clear();
	g_measured = false;
}

bool NotificationBar::HasPending()
{
	return !g_pending.empty();
}

void NotificationBar::Draw()
{
	if (g_pending.empty())
		return;

	const std::string& text = g_pending.front();

	if (ImGui::BeginMainMenuBar())
	{
		g_screenWidth = ImGui::GetWindowWidth();

		if (!g_measured)
			Measure(text);

		ImGui::SetCursorPosX(g_offset);
		ImGui::TextUnformatted(text.c_str());

		ImGui::EndMainMenuBar();
	}

	if (!g_measured)
		return;

	g_offset -= ImGui::GetIO().DeltaTime * ((g_screenWidth + g_textWidth) / kSecondsToCross);

	if (g_offset > -g_textWidth)
		return;

	g_pending.pop_front();
	g_measured = false;
}
