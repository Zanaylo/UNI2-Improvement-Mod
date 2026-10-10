#pragma once

#include "Game/Menus/MenuInput.h"
#include "Game/Menus/OptionMenu.h"

#include <atomic>

class OptionScreenState
{
public:
	enum Action
	{
		Action_None,
		Action_Close,
	};

	static constexpr int kConfirmRow = 0;
	static constexpr int kResetRow = 1;
	static constexpr int kReturnRow = 2;
	static constexpr int kActions = 3;

	explicit OptionScreenState(OptionMenu::IClient& client);

	void Open();
	Action Handle(const MenuInput::State& input);

	OptionMenu::IClient& Client() const;
	int Settings() const;
	int RowCount() const;
	int Cursor() const;
	int Value(int row) const;
	bool IsChanged(int row) const;

private:
	void Revert();
	void Reset();
	Action OnConfirm();
	void Move(int delta);
	void Change(int delta);

	OptionMenu::IClient& m_client;
	std::atomic<int> m_cursor;
	std::atomic<int> m_values[OptionMenu::kMaxRows];
	int m_opened[OptionMenu::kMaxRows];
};
