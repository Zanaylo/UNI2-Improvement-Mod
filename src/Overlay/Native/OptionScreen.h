#pragma once

#include "Game/Menus/OptionMenu.h"
#include "Game/Menus/TrainingMenu.h"
#include "Overlay/Native/OptionScreenState.h"

#include <d3d9.h>

#include <atomic>

class OptionScreen : public TrainingMenu::IModal
{
public:
	explicit OptionScreen(OptionMenu::IClient& client);

	void Open();

	bool IsOpen() const override;
	void Close() override;
	void Update(const MenuInput::State& input) override;

	void Render(IDirect3DDevice9* device) const;

private:
	OptionScreenState m_state;
	std::atomic<bool> m_open;
};
