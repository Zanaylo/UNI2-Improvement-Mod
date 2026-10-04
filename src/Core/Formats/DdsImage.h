#pragma once

#include <cstdint>
#include <vector>

namespace DdsImage
{
	constexpr size_t kHeaderBytes = 128;

	struct Image
	{
		std::vector<uint8_t> pixels;
		int width = 0;
		int height = 0;
	};

	bool Decode(const std::vector<uint8_t>& dds, Image& out);
	bool DecodePlain(const std::vector<uint8_t>& dds, Image& out);

	std::vector<uint8_t> EncodeArgb(const Image& image);
	std::vector<uint8_t> EncodeDxt(const Image& image);
}
