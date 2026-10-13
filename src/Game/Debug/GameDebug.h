#pragma once

class FileIndex;

namespace GameDebug
{
	struct Option
	{
		bool* value;
		const char* key;
		const char* label;
		const char* help;
		bool changesTheGame;
	};

	bool IsEnabled();

	void Prepare();
	void Layer(FileIndex& into);
	void OnFrame();

	int OptionCount();
	const Option& OptionAt(int index);
	void Save(int index);

	bool HoldsBackGameChanges();
	const char* StatusText();
}
