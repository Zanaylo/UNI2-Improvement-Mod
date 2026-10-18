#pragma once

#include "Core/Formats/Markdown.h"

#include <vector>

namespace MarkdownView
{
	void Draw(const std::vector<Markdown::Block>& blocks);
}
