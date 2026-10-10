#include "Core/Harness/DrawCommands.h"

#include "D3D9/Draw/DrawQueueCensus.h"
#include "D3D9/Draw/LayerFadeProbe.h"

#include <cstdlib>

namespace {

constexpr size_t kFadeWords = 4;

std::string RunCensus(const std::vector<std::string>& words)
{
	if (words.size() >= 2 && words[1] == "start")
		return DrawQueueCensus::Start() ? "ok recording" : "error the draw queue hook is not available";

	return DrawQueueCensus::Finish();
}

std::string RunFade(const std::vector<std::string>& words)
{
	if (words.size() < kFadeWords)
	{
		LayerFadeProbe::Clear();
		return "ok cleared";
	}

	const uint32_t first = static_cast<uint32_t>(strtoul(words[1].c_str(), nullptr, 0));
	const uint32_t last = static_cast<uint32_t>(strtoul(words[2].c_str(), nullptr, 0));
	const int percent = atoi(words[3].c_str());

	return LayerFadeProbe::Set(first, last, percent) ? "ok fading" : "error the draw queue hook is not available";
}

}

bool DrawCommands::Execute(const std::vector<std::string>& words, std::string& reply)
{
	if (words[0] == "census")
	{
		reply = RunCensus(words);
		return true;
	}

	if (words[0] != "fade")
		return false;

	reply = RunFade(words);
	return true;
}
