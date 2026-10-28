#pragma once

#include <cstddef>
#include <cstdint>

namespace ShaderSamplers
{
	constexpr uint32_t kEveryStage = 0xffffffffu;

	uint32_t Declared(const uint32_t* function, size_t tokens);
	uint32_t Enabled(const uint32_t* colourOps, size_t stages);
}
