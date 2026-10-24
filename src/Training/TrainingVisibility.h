#pragma once

#include "Training/TrainingItems.h"

namespace TrainingVisibility
{
	bool IsAvailable(TrainingItems::Item item);
	bool IsShown(TrainingItems::Item item);
	void SetShown(TrainingItems::Item item, bool shown);

	void Enforce();
}
