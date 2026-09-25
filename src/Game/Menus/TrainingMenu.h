#pragma once

#include "Game/Menus/MenuInput.h"

namespace TrainingMenu
{
	constexpr int kNoValue = -1;
	constexpr int kMaxChoices = 8;
	constexpr int kMaxItems = 16;

	struct PageSpec
	{
		const char* title;
		const char* info;
	};

	struct ItemSpec
	{
		int id;
		const char* word;
		const char* info;
		const char* const* choices;
		int choiceCount;
		int value;
	};

	class IClient
	{
	public:
		virtual ~IClient() = default;

		virtual PageSpec Page() const = 0;
		virtual int ItemCount() const = 0;
		virtual ItemSpec Item(int index) const = 0;

		virtual void BeforeUpdate() = 0;
		virtual void AfterUpdate() = 0;

		virtual bool OnOpenPicker(int id) = 0;
	};

	class IModal
	{
	public:
		virtual ~IModal() = default;

		virtual bool IsOpen() const = 0;
		virtual void Close() = 0;
		virtual void Update(const MenuInput::State& input) = 0;
	};

	bool Install(IClient* client);

	void OpenModal(IModal* modal);

	int GetValue(int id);
	bool SetValue(int id, int value);

	bool IsActive();
	bool IsHeld();

	const char* StatusText();
}
