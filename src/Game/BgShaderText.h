#pragma once

#include <string>

namespace BgShaderText
{
	struct Result
	{
		int black;
		int contrast;
		int flow;
		int fade;
	};

	bool IsBackground(const std::string& source);

	bool Rewrite(const std::string& source, int blackRegister, int contrastRegister,
		int flowRegister, int lampRegister, float gameBlack, std::string& out, Result& result);

	std::string Assignment(const std::string& source);
}
