#pragma once

#include "Game/Stages/FbxExWriter.h"

#include <cstdint>
#include <vector>

namespace FbxExReader
{
	bool Read(const std::vector<uint8_t>& blob, FbxExWriter::Model& out);
}
