#include "Training/TrainingItems.h"

namespace {

const char* const kNames[TrainingItems::Item_COUNT] = {
	"Training shortcut",
	"Hitbox viewer",
	"Frame meter",
	"GRD popups",
	"GRD timer",
	"Health values",
	"Proration info",
	"Hide the HUD",
	"Hide characters and effects",
	"Player Control",
	"Pause and frame step",
	"Auto pause",
	"Timing",
	"Dummy script",
	"Extras",
	"Hitbox types",
	"Frame meter options",
};

bool Known(TrainingItems::Item item)
{
	return item >= 0 && item < TrainingItems::Item_COUNT;
}

unsigned Bit(TrainingItems::Item item)
{
	return 1u << static_cast<unsigned>(item);
}

}

const char* TrainingItems::Name(Item item)
{
	if (!Known(item))
		return "Unknown";

	return kNames[item];
}

bool TrainingItems::Shown(unsigned hiddenMask, Item item)
{
	if (!Known(item))
		return true;

	return (hiddenMask & Bit(item)) == 0;
}

unsigned TrainingItems::WithShown(unsigned hiddenMask, Item item, bool shown)
{
	if (!Known(item))
		return hiddenMask;

	if (shown)
		return hiddenMask & ~Bit(item);

	return hiddenMask | Bit(item);
}

bool TrainingItems::Available(bool moduleOn, unsigned hiddenMask, Item item)
{
	return moduleOn && Shown(hiddenMask, item);
}
