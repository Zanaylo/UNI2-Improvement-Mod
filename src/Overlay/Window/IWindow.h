// Base for every overlay window. Each one is registered in WindowContainer by WindowType.

#pragma once

#include <imgui.h>

#include <string>

class IWindow
{
public:
	IWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);
	virtual ~IWindow() = default;

	void Update();

	bool IsOpen() const { return m_isOpen; }
	void Open() { m_isOpen = true; }
	void Close() { m_isOpen = false; }
	void Toggle() { m_isOpen = !m_isOpen; }

	const std::string& GetTitle() const { return m_title; }

protected:
	virtual void Draw() = 0;
	virtual void BeforeDraw() {}
	virtual void AfterDraw() {}

	virtual bool GrowsToFitContent() const { return false; }

	std::string m_title;
	bool m_isOpen;
	bool m_closable;
	ImGuiWindowFlags m_windowFlags;
};
