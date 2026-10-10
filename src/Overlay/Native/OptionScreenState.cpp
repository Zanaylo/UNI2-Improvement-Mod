#include "Overlay/Native/OptionScreenState.h"

namespace {

int Wrap(int value, int count)
{
	return count <= 0 ? 0 : (value % count + count) % count;
}

}

OptionScreenState::OptionScreenState(OptionMenu::IClient& client)
	: m_client(client)
	, m_cursor(0)
	, m_values{}
	, m_opened{}
{
}

void OptionScreenState::Open()
{
	for (int row = 0; row < Settings(); ++row)
	{
		m_opened[row] = m_client.Current(row);
		m_values[row].store(m_opened[row]);
	}

	m_cursor.store(0);
}

OptionMenu::IClient& OptionScreenState::Client() const
{
	return m_client;
}

int OptionScreenState::Settings() const
{
	const int rows = m_client.RowCount();

	return rows < OptionMenu::kMaxRows ? rows : OptionMenu::kMaxRows;
}

int OptionScreenState::RowCount() const
{
	return Settings() + kActions;
}

int OptionScreenState::Cursor() const
{
	return m_cursor.load();
}

int OptionScreenState::Value(int row) const
{
	return row >= 0 && row < Settings() ? m_values[row].load() : 0;
}

bool OptionScreenState::IsChanged(int row) const
{
	return row >= 0 && row < Settings() && Value(row) != m_client.Default(row);
}

void OptionScreenState::Revert()
{
	for (int row = 0; row < Settings(); ++row)
	{
		if (Value(row) != m_opened[row])
			m_client.Apply(row, m_opened[row]);
	}
}

void OptionScreenState::Reset()
{
	for (int row = 0; row < Settings(); ++row)
	{
		m_values[row].store(m_client.Default(row));
		m_client.Apply(row, Value(row));
	}
}

OptionScreenState::Action OptionScreenState::OnConfirm()
{
	const int cursor = Cursor();

	if (cursor == Settings() + kResetRow)
	{
		Reset();
		return Action_None;
	}

	if (cursor == Settings() + kReturnRow)
		Revert();

	return Action_Close;
}

void OptionScreenState::Move(int delta)
{
	m_cursor.store(Wrap(Cursor() + delta, RowCount()));
}

void OptionScreenState::Change(int delta)
{
	const int row = Cursor();

	if (row >= Settings())
		return;

	const int last = m_client.Row(row).choiceCount - 1;
	const int wanted = Value(row) + delta;
	const int value = wanted < 0 ? 0 : (wanted > last ? last : wanted);

	if (value == Value(row))
		return;

	m_values[row].store(value);
	m_client.Apply(row, value);
}

OptionScreenState::Action OptionScreenState::Handle(const MenuInput::State& input)
{
	if (input.cancel)
	{
		Revert();
		return Action_Close;
	}

	if (input.confirm)
		return OnConfirm();

	if (input.lever == MenuInput::kLeverUp || input.lever == MenuInput::kLeverDown)
		Move(input.lever == MenuInput::kLeverDown ? 1 : -1);
	else if (input.lever == MenuInput::kLeverLeft || input.lever == MenuInput::kLeverRight)
		Change(input.lever == MenuInput::kLeverRight ? 1 : -1);

	return Action_None;
}