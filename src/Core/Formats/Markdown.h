#pragma once

#include <string>
#include <vector>

namespace Markdown
{
	enum class Style
	{
		Normal,
		Bold,
		Strike,
		Code,
		Link,
	};

	enum class Kind
	{
		Paragraph,
		Heading,
		Bullet,
		Numbered,
		Quote,
		Rule,
		Code,
		Gap,
	};

	struct Piece
	{
		std::string text;
		std::string url;
		Style style = Style::Normal;
		bool spaceBefore = false;
	};

	struct Block
	{
		Kind kind = Kind::Paragraph;
		int level = 0;
		std::string marker;
		std::vector<Piece> pieces;
	};

	std::vector<Block> Parse(const std::string& text);
}
