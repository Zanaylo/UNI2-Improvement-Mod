// The size the finished frame is drawn at before Direct3D fits it to the window.
//
// Two tabs ask for it from opposite directions - POTATO MODE downwards, Improvements upwards - so it
// is derived here rather than written by either of them, and a level is the only thing they own.
// POTATO MODE wins while it is on, which is why the Improvements tab is not offered then.

#pragma once

namespace PresentSize
{
	void Refresh();
}
