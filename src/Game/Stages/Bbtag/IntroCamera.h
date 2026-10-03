#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace IntroCamera
{
	constexpr int kFrames = 600;

	std::string FileName(const std::string& stage);

	std::vector<uint8_t> Still(const std::string& stage);
}
