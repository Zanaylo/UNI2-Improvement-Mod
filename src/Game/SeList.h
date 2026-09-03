#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SeList
{
	struct Row
	{
		std::string stem;
		std::string note;
		std::string variable;
		int path;
	};

	struct File
	{
		std::vector<std::string> paths;
		std::vector<Row> rows;
	};

	std::string Text(const std::vector<uint8_t>& bytes);

	void Parse(const std::string& text, File& out);

	std::string NoteKey(const std::string& note);

	std::string Number(const std::string& stem);
}
