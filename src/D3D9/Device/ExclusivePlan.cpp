#include "D3D9/Device/ExclusivePlan.h"

namespace {

bool IsSet(ExclusivePlan::Size size)
{
	return size.width > 0 && size.height > 0;
}

bool Matches(const D3DPRESENT_PARAMETERS& parameters, ExclusivePlan::Size size)
{
	return parameters.BackBufferWidth == size.width && parameters.BackBufferHeight == size.height;
}

}

bool ExclusivePlan::Wants(const D3DPRESENT_PARAMETERS& display, Size wanted)
{
	if (display.Windowed || !IsSet(wanted))
		return false;

	if (wanted.width < display.BackBufferWidth || wanted.height < display.BackBufferHeight)
		return false;

	return !Matches(display, wanted);
}

ExclusivePlan::Size ExclusivePlan::DisplayFor(const D3DPRESENT_PARAMETERS& asked, Size drawn, Size display)
{
	if (IsSet(drawn) && IsSet(display) && Matches(asked, drawn))
		return display;

	return { asked.BackBufferWidth, asked.BackBufferHeight };
}
