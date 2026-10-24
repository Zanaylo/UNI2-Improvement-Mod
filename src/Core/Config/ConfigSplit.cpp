#include "Core/Config/ConfigSplit.h"

#include "Core/Config/ConfigLayout.h"

namespace {

constexpr char kNewLine[] = "\r\n";
constexpr char kBlankLine[] = "\r\n\r\n";
constexpr size_t kBlankLineLength = sizeof(kBlankLine) - 1;

bool EndsWithBlankLine(const std::string& text)
{
	return text.size() >= kBlankLineLength && text.compare(text.size() - kBlankLineLength, kBlankLineLength, kBlankLine) == 0;
}

void AppendSection(std::string& file, const std::string& section)
{
	if (!file.empty() && !EndsWithBlankLine(file))
		file += kNewLine;

	file += section;
}

}

std::map<std::string, std::string> ConfigSplit::Split(const IniDocument& whole)
{
	std::map<std::string, std::string> files;

	for (const std::string& name : whole.SectionNames())
		AppendSection(files[ConfigLayout::FileFor(name.c_str())], whole.SectionText(name));

	return files;
}
