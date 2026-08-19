// Combo boxes that behave: the wheel steps through the list while the closed combo is hovered, and
// opening one lands on what is already selected instead of at the top.

#pragma once

namespace ComboNav
{
	int WheelSteps();

	void KeepSelectedInView(bool selected);
}
