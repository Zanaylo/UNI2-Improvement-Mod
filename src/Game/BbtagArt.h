#pragma once

#include <cstdint>
#include <vector>

namespace BbtagArt
{
	struct Size
	{
		int width;
		int height;
	};

	bool Measure(const std::vector<uint8_t>& blob, Size& out);

	bool Transparent(const std::vector<uint8_t>& blob);

	bool Uniform(const std::vector<uint8_t>& blob);

	void AlphaMix(const std::vector<uint8_t>& blob, float& clear, float& fine, float& solid);

	float Greyscale(const std::vector<uint8_t>& blob);

	void Backing(const std::vector<uint8_t>& blob, float& share, float& level);

	bool Lit(const std::vector<uint8_t>& blob);

	bool Haloed(const std::vector<uint8_t>& blob, const float box[4]);
}
