#pragma once

#include "Core/Formats/Markdown.h"
#include "Overlay/Framework/IWindow.h"

#include <string>
#include <vector>

class UpdateNotifierWindow : public IWindow
{
public:
	UpdateNotifierWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	struct ReleaseNotes
	{
		std::string tag;
		std::string title;
		std::string page;
		std::string date;
		std::vector<Markdown::Block> notes;
	};

	void Refresh();
	void DrawHeader() const;
	void DrawNotes();
	void DrawRelease(const ReleaseNotes& release) const;
	void DrawButtons();
	void DrawPrimaryButton(const ImVec2& size);

	float BottomReserve() const;

	std::vector<ReleaseNotes> m_releases;
	long m_revision = -1;
	bool m_canInstall = false;
	bool m_scrollToTop = false;
};
