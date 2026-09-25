#pragma once

namespace SubtitleTable
{
	constexpr int kStemMax = 40;
	constexpr int kNoteMax = 192;
	constexpr int kTextMax = 192;
	constexpr int kNameMax = 64;
	constexpr int kCharacters = 32;

	struct Line
	{
		char stem[kStemMax];
		char note[kNoteMax];
		char text[kTextMax];
		int index;
	};

	void Load();

	int PackCount();
	const char* PackAt(int index);

	const char* Chosen();
	bool Choose(const char* name);
	bool Create(const char* name);

	bool IsDirty();
	bool Save();

	bool Attach(int chara);
	bool IsAttached(int chara);

	int Rows(int chara);
	const Line* Row(int chara, int index);
	bool Write(int chara, int index, const char* text);

	int Written(int chara);

	bool Lookup(int chara, int index, char* out, int size);

	bool ExportTo(const char* path);
	bool ImportFrom(const char* path);

	const char* FolderPath();
	const char* StatusText();
}
