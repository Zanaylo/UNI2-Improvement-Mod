#pragma once

namespace PortraitFrames
{
	enum Frame
	{
		Frame_Select,
		Frame_Versus,
		Frame_Closeup,
		Frame_Card,
		Frame_Winner,
		Frame_Menu,
		Frame_GaugeLeft,
		Frame_GaugeRight,
		Frame_Network,
		Frame_Count,
	};

	struct Transform
	{
		double scale;
		double x;
		double y;
		bool mirrored;
	};

	struct Chara
	{
		int chara;
		int baseWidth;
		Transform frames[Frame_Count];
	};

	const Chara* Of(int chara);
}
