#pragma once

#include <string>

namespace BbtagDefaults
{
	struct Look
	{
		const char* stage;
		float size;
		float contrast;
		float glow;
	};

	const Look* Of(const std::string& stage);
}
