#pragma once

#include "Overlay/Guides/GuideContent.h"

namespace GuideImGui
{
	enum Layout
	{
		Layout_Grid,
		Layout_List
	};

	void Draw(const GuideContent& content, Layout layout);
}
