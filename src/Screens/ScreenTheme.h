#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ScreenTheme
{
	struct Layer
	{
		std::string pattern;
		std::string file;
		std::string cursor;
		std::string tag[2];
		float tagX[2];
		float tagY[2];
		int count;
		int fps;
		float x;
		float y;
		float zoom;
		float spreadX;
		float spreadY;
		uint32_t tint[2][2];
		int side;
		bool part;
		bool hold;
	};

	struct Screen
	{
		std::string name;
		std::string pat;
		std::vector<uint32_t> scenes;
		std::vector<Layer> layers;
		std::vector<std::string> charaCodes;
		float designWidth;
		float designHeight;
		uintptr_t cursor[2];
		uintptr_t statePointer;
		uint32_t stateStride;
		uint32_t stateField;
		bool originTop;
		bool cover;
	};

	struct Theme
	{
		std::string id;
		std::string name;
		std::string author;
		std::string source;
		std::vector<Screen> screens;
	};

	void Reload();

	int Count();
	const Theme* Get(int index);

	int ActiveIndex();
	const Theme* Active();

	bool Apply(int index);
	void Clear();

	const Screen* ScreenForScene(uint32_t scene);

	std::string FilePath(const Theme& theme, const std::string& relative);

	const char* Root();
	const char* StatusText();
}
