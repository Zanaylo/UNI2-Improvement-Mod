#include "Game/Stages/Mbaa/MbaaTimeline.h"

#include "Game/Stages/Mbaa/MbaaScene.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <unordered_set>

namespace {

constexpr uint32_t kSeed = 0x4d424141;
constexpr int kWarm = 600;
constexpr int kMostPeriod = 3600;
constexpr int kWindow = 1200;
constexpr int kMostLoop = 7200;
constexpr int kMostRecord = 3 * kMostLoop;
constexpr int kMostRamps = 512;
constexpr int kLampFull = 1000;
constexpr int kOpaque = 255;
constexpr int kRampSlack = 4;
constexpr int kBits = 64;

struct Track
{
	int born = -1;
	bool ended = false;
	std::vector<std::pair<int, MbaaScene::Shown>> seen;
};

int FindPeriod(const MbaaBg::File& file, bool& rolled)
{
	MbaaScene probe(file, kSeed);

	for (int i = 0; i < kWarm; ++i)
		probe.Step();

	const uint64_t start = probe.Hash();

	for (int period = 1; period <= kMostPeriod; ++period)
	{
		probe.Step();

		if (probe.Hash() == start)
		{
			rolled = probe.Rolled();
			return period;
		}
	}

	rolled = probe.Rolled();
	return 0;
}

bool InWindow(const Track& track, int loop)
{
	return track.born > 0 && track.born <= loop;
}

int Longest(const std::map<int, Track>& tracks, int loop, bool& settled)
{
	int longest = 1;
	settled = true;

	for (const std::pair<const int, Track>& track : tracks)
	{
		if (!InWindow(track.second, loop) || track.second.seen.empty())
			continue;

		settled = settled && track.second.ended;
		longest = (std::max)(longest, track.second.seen.back().first - track.second.born + 1);
	}

	return longest;
}

int Grown(int period, int longest)
{
	const int loops = (longest + period - 1) / period;

	return (std::min)(kMostLoop, (std::max)(period, period * loops));
}

void Record(MbaaScene& scene, int tick, std::map<int, Track>& tracks)
{
	std::vector<MbaaScene::Shown> shown;
	scene.Visible(shown);

	std::vector<int> living;
	scene.Living(living);
	const std::unordered_set<int> alive(living.begin(), living.end());

	for (int instance : living)
	{
		Track& track = tracks[instance];

		if (track.born < 0)
			track.born = tick;
	}

	for (const MbaaScene::Shown& one : shown)
		tracks[one.instance].seen.emplace_back(tick, one);

	for (std::pair<const int, Track>& track : tracks)
		track.second.ended = track.second.ended || alive.count(track.first) == 0;
}

void AddUnits(int object, const std::vector<std::pair<int, MbaaScene::Shown>>& seen, int from, int until,
	int loop, std::vector<MbaaTimeline::Unit>& out)
{
	std::map<int, MbaaTimeline::Unit> byBlend;

	for (const std::pair<int, MbaaScene::Shown>& entry : seen)
	{
		if (entry.first < from || entry.first >= until)
			continue;

		MbaaTimeline::Unit& unit = byBlend[entry.second.blend];
		unit.object = object;
		unit.blend = entry.second.blend;
		unit.samples.push_back({ entry.first % loop, entry.second.image, entry.second.x, entry.second.y,
			entry.second.alpha });
	}

	for (std::pair<const int, MbaaTimeline::Unit>& unit : byBlend)
	{
		std::sort(unit.second.samples.begin(), unit.second.samples.end(),
			[](const MbaaTimeline::Sample& one, const MbaaTimeline::Sample& other) { return one.tick < other.tick; });
		out.push_back(unit.second);
	}
}

bool Linear(const std::vector<int>& value, int from, int to)
{
	for (int t = from + 1; t < to; ++t)
	{
		const int expected = value[static_cast<size_t>(from)]
			+ (value[static_cast<size_t>(to)] - value[static_cast<size_t>(from)]) * (t - from) / (to - from);

		if (abs(expected - value[static_cast<size_t>(t)]) > kRampSlack)
			return false;
	}

	return true;
}

std::vector<int> Filled(const std::vector<int>& alpha)
{
	std::vector<int> out(alpha.size(), -1);
	int held = -1;

	for (size_t i = 0; i < alpha.size() * 2 && !alpha.empty(); ++i)
	{
		const int value = alpha[i % alpha.size()];

		if (value >= 0)
			held = value * kLampFull / kOpaque;

		if (out[i % alpha.size()] < 0)
			out[i % alpha.size()] = held;
	}

	for (int& value : out)
		value = value < 0 ? kLampFull : value;

	return out;
}

bool Overlaps(const std::vector<uint64_t>& busy, const MbaaTimeline::Unit& unit)
{
	for (const MbaaTimeline::Sample& sample : unit.samples)
	{
		if ((busy[static_cast<size_t>(sample.tick / kBits)] >> (sample.tick % kBits)) & 1u)
			return true;
	}

	return false;
}

void Occupy(std::vector<uint64_t>& busy, const MbaaTimeline::Unit& unit)
{
	for (const MbaaTimeline::Sample& sample : unit.samples)
		busy[static_cast<size_t>(sample.tick / kBits)] |= uint64_t(1) << (sample.tick % kBits);
}

std::vector<std::vector<int>> Assign(const std::vector<MbaaTimeline::Unit>& units, const std::vector<int>& order,
	int period, int most)
{
	std::vector<std::vector<int>> lanes;
	std::vector<std::vector<uint64_t>> busy;
	const size_t words = static_cast<size_t>(period / kBits + 1);

	for (int index : order)
	{
		const MbaaTimeline::Unit& unit = units[static_cast<size_t>(index)];
		size_t lane = 0;

		while (lane < lanes.size() && Overlaps(busy[lane], unit))
			++lane;

		if (lane == lanes.size())
		{
			if (static_cast<int>(lanes.size()) >= most)
				continue;

			lanes.emplace_back();
			busy.emplace_back(words, 0);
		}

		lanes[lane].push_back(index);
		Occupy(busy[lane], unit);
	}

	return lanes;
}

}

bool MbaaTimeline::Build(const MbaaBg::File& file, Timeline& out)
{
	out = Timeline();

	bool rolled = false;
	const int period = FindPeriod(file, rolled);
	out.exact = period > 0 && !rolled;

	MbaaScene scene(file, kSeed);
	const int placed = scene.Instances();

	for (int i = 0; i < kWarm; ++i)
		scene.Step();

	int loop = out.exact ? period : kWindow;
	std::map<int, Track> tracks;

	for (int tick = 0; tick < kMostRecord; ++tick)
	{
		Record(scene, tick, tracks);

		if (tick >= loop)
		{
			bool settled = false;
			const int longest = Longest(tracks, loop, settled);
			const int grown = out.exact ? Grown(period, longest) : loop;

			if (grown > loop)
				loop = grown;
			else if (settled || tick >= 2 * loop)
				break;
		}

		scene.Step();
	}

	out.period = loop;

	for (const std::pair<const int, Track>& entry : tracks)
	{
		const Track& track = entry.second;

		if (track.seen.empty())
			continue;

		const int object = track.seen.front().second.object;

		if (entry.first <= placed && track.born == 0)
			AddUnits(object, track.seen, 0, loop, loop, out.units);
		else if (InWindow(track, loop))
			AddUnits(object, track.seen, track.born, track.born + loop, loop, out.units);
	}

	return !out.units.empty();
}

std::vector<std::vector<int>> MbaaTimeline::Lanes(const std::vector<Unit>& units, int period, int most)
{
	std::vector<int> every(units.size());

	for (size_t i = 0; i < units.size(); ++i)
		every[i] = static_cast<int>(i);

	const std::vector<std::vector<int>> free = Assign(units, every, period, static_cast<int>(units.size()));

	if (static_cast<int>(free.size()) <= most)
		return free;

	const size_t stride = (free.size() + static_cast<size_t>(most) - 1) / static_cast<size_t>(most);
	std::vector<int> thinned;

	for (size_t i = 0; i < units.size(); i += stride)
		thinned.push_back(static_cast<int>(i));

	return Assign(units, thinned, period, most);
}

bool MbaaTimeline::Ramps(const std::vector<int>& alpha, std::vector<BbtagScript::Ramp>& out)
{
	out.clear();

	if (alpha.empty())
		return false;

	const std::vector<int> value = Filled(alpha);
	const int last = static_cast<int>(value.size()) - 1;

	out.push_back({ 0, value[0], 0 });

	for (int from = 0; from < last;)
	{
		int to = from + 1;

		while (to < last && Linear(value, from, to + 1))
			++to;

		out.push_back({ from, value[static_cast<size_t>(to)], to - from });
		from = to;

		if (static_cast<int>(out.size()) > kMostRamps)
			return false;
	}

	return true;
}
