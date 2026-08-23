#include "Game/PresentSize.h"

#include "Core/interfaces.h"
#include "Game/Improvements.h"
#include "Game/PotatoMode.h"

void PresentSize::Refresh()
{
	int width = 0;
	int height = 0;

	if (!PotatoMode::GetPresentSize(width, height))
		Improvements::GetPresentSize(width, height);

	g_modVals.presentWidth = width;
	g_modVals.presentHeight = height;
}
