#include "Game/Stages/Mbaa/MbaaAtlas.h"

#include <algorithm>
#include <numeric>

namespace {

constexpr int kGap = 4;
constexpr int kSmallest = 4;
constexpr int kShrinks = 5;

int Aligned(int value)
{
	return (value + kGap - 1) / kGap * kGap;
}

int PowerOfTwo(int value)
{
	int side = kSmallest;

	while (side < value)
		side *= 2;

	return side;
}

bool Shelves(const std::vector<MbaaAtlas::Size>& sizes, const std::vector<size_t>& order, int width, int most,
	MbaaAtlas::Layout& out)
{
	out.rects.assign(sizes.size(), ImageOps::Rect{ 0, 0, 0, 0 });

	int x = 0;
	int y = 0;
	int shelf = 0;

	for (size_t index : order)
	{
		const MbaaAtlas::Size& size = sizes[index];

		if (size.width > width)
			return false;

		if (x + size.width > width)
		{
			x = 0;
			y += Aligned(shelf) + kGap;
			shelf = 0;
		}

		out.rects[index] = ImageOps::Rect{ x, y, size.width, size.height };
		x += Aligned(size.width) + kGap;
		shelf = (std::max)(shelf, size.height);
	}

	out.width = width;
	out.height = PowerOfTwo(y + shelf);

	return out.height <= most;
}

bool PackAt(const std::vector<MbaaAtlas::Size>& sizes, int most, MbaaAtlas::Layout& out)
{
	std::vector<size_t> order(sizes.size());
	std::iota(order.begin(), order.end(), size_t(0));
	std::stable_sort(order.begin(), order.end(),
		[&sizes](size_t one, size_t other) { return sizes[one].height > sizes[other].height; });

	int widest = 0;

	for (const MbaaAtlas::Size& size : sizes)
		widest = (std::max)(widest, size.width);

	bool found = false;
	MbaaAtlas::Layout best;

	for (int width = PowerOfTwo(widest); width <= most; width *= 2)
	{
		MbaaAtlas::Layout tried;

		if (!Shelves(sizes, order, width, most, tried))
			continue;

		if (found && static_cast<long long>(tried.width) * tried.height >= static_cast<long long>(best.width) * best.height)
			continue;

		best = tried;
		found = true;
	}

	if (found)
		out = best;

	return found;
}

}

bool MbaaAtlas::Pack(const std::vector<Size>& cells, int most, Layout& out)
{
	out = Layout();

	if (cells.empty())
		return false;

	float scale = 1.0f;

	for (int shrink = 0; shrink <= kShrinks; ++shrink, scale *= 0.5f)
	{
		std::vector<Size> sized;

		for (const Size& cell : cells)
		{
			sized.push_back(Size{ (std::max)(1, static_cast<int>(cell.width * scale)),
				(std::max)(1, static_cast<int>(cell.height * scale)) });
		}

		if (!PackAt(sized, most, out))
			continue;

		out.scale = scale;
		return true;
	}

	return false;
}
