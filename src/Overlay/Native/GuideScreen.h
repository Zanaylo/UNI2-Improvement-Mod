#pragma once

#include "Game/Menus/TrainingMenu.h"
#include "Overlay/Guides/GuideContent.h"

#include <d3d9.h>

#include <atomic>

class GuideScreen : public TrainingMenu::IModal
{
public:
	typedef const GuideContent& (*ContentFn)();

	explicit GuideScreen(ContentFn content);

	void Open();

	bool IsOpen() const override;
	void Close() override;
	void Update(const MenuInput::State& input) override;

	void Render(IDirect3DDevice9* device) const;

private:
	void TurnPage(int delta);
	void MoveRow(int delta);

	ContentFn m_content;
	std::atomic<bool> m_open;
	std::atomic<int> m_page;
	std::atomic<int> m_row;
	std::atomic<int> m_scroll;
};
