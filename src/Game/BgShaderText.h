#pragma once

#include <string>

namespace BgShaderText
{
	struct Result
	{
		int black;
		int contrast;
	};

	bool IsBackground(const std::string& source);

	bool Rewrite(const std::string& source, int blackRegister, int contrastRegister,
		float gameBlack, std::string& out, Result& result);

	std::string Assignment(const std::string& source);
}
