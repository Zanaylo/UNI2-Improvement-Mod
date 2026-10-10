#pragma once

namespace OptionMenu
{
	constexpr int kMaxRows = 16;

	struct RowSpec
	{
		const char* word;
		const char* info;
		const char* const* choices;
		int choiceCount;
	};

	class IClient
	{
	public:
		virtual ~IClient() = default;

		virtual const char* EntryWord() const = 0;
		virtual const char* EntryInfo() const = 0;
		virtual const char* Title() const = 0;

		virtual int RowCount() const = 0;
		virtual RowSpec Row(int index) const = 0;

		virtual int Current(int index) const = 0;
		virtual int Default(int index) const = 0;
		virtual void Apply(int index, int value) = 0;
	};

	struct TitleView
	{
		int top;
		const char* text;
	};

	bool Install(IClient* client);
	bool ShownTitle(TitleView& out);

	const char* StatusText();
}
