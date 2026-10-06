#include "Palette/ColourSlotStep.h"

namespace {

constexpr uint8_t kLeverDown = 2;
constexpr uint8_t kLeverLeft = 4;
constexpr uint8_t kLeverRight = 6;
constexpr uint8_t kLeverUp = 8;

constexpr int kSingleStep = 1;
constexpr int kPageStep = 10;

int Wrap(int value, int count)
{
	const int remainder = value % count;

	return remainder < 0 ? remainder + count : remainder;
}

}

int ColourSlotStep::DeltaOf(uint8_t lever)
{
	switch (lever)
	{
	case kLeverRight:
		return kSingleStep;
	case kLeverLeft:
		return -kSingleStep;
	case kLeverDown:
		return kPageStep;
	case kLeverUp:
		return -kPageStep;
	default:
		return 0;
	}
}

ColourSlotStep::Result ColourSlotStep::Step(int listCount, Position from, int extendedCount, int delta)
{
	const Result gameMove = { true, from.listIndex, kNoExtended };

	if (listCount <= 0 || extendedCount <= 0 || delta == 0)
		return gameMove;

	const int extended = from.extended < extendedCount ? from.extended : extendedCount;
	const int landing = from.listIndex + delta;

	if (extended == kNoExtended && landing >= 0 && landing < listCount)
		return gameMove;

	const int position = extended == kNoExtended ? from.listIndex : listCount - 1 + extended;
	const int target = Wrap(position + delta, listCount + extendedCount);

	if (target < listCount)
		return { false, target, kNoExtended };

	return { false, kParkIndex, target - listCount + 1 };
}

int ColourSlotStep::LeadIn(int listCount, int target, int delta)
{
	if (listCount <= 0)
		return 0;

	const int step = delta < 0 ? -kSingleStep : kSingleStep;

	return Wrap(target - step, listCount);
}
