#include "Game/Display/PresentSize.h"

#include "Core/Config/interfaces.h"
#include "Game/Display/Improvements.h"
#include "Game/Display/PotatoMode.h"

void PresentSize::Refresh()
{
	int width = 0;
	int height = 0;

	if (!PotatoMode::GetPresentSize(width, height))
		Improvements::GetPresentSize(width, height);

	g_modVals.presentWidth = width;
	g_modVals.presentHeight = height;
}
