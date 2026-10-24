#pragma once

#include <string>

namespace CharaShaderText
{
	constexpr int kTexelRegister = 180;

	bool Rewrite(const std::string& source, std::string& out, int& replaced);
}
