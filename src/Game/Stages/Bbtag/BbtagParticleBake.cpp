#include "Game/Stages/Bbtag/BbtagParticleBake.h"

#include "Game/Stages/Bbtag/BbtagCamera.h"
#include "Screens/PatWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <set>

namespace {

constexpr uint32_t kSeed = 1;
constexpr int kLives = 8;
constexpr int kStep = 3;
constexpr int kLanesPerBand = 2;
constexpr int kEntryLimit = 99;
constexpr size_t kPatLimit = 8u * 1024u * 1024u;

constexpr double kFrameHeight = 720.0;
constexpr double kLayerFocal = 1735.0;
constexpr double kNearest = 1.0;
constexpr double kTau = 2.0 * BbtagCamera::kPi;

constexpr int kPrioBehind = 271;
constexpr int kPrioFront = 402;
constexpr int kSpritePriority = 10;
constexpr uint8_t kAdditive = 1;
constexpr float kHiddenZoom = 0.001f;
constexpr int kAtlasSides[] = { 256, 512 };
constexpr int kUvSpace = 256;
constexpr int kUnbounded = 1 << 30;
constexpr const char* kPatSuffix = "_particles.pat";

constexpr uint32_t kFnvBasis = 0x811c9dc5u;
constexpr uint32_t kFnvPrime = 0x01000193u;
constexpr uint32_t kDiceFallback = 0x9e3779b9u;
constexpr double kDiceSpan = 4294967296.0;
constexpr uint32_t kCountDraw = 0xc0;
constexpr uint32_t kLaneDraw = 0x100;

constexpr uint32_t kGroupRespawns = 0x10;
constexpr uint32_t kGroupStaggered = 0x60;

constexpr uint32_t kSpriteDepthTest = 0x4;
constexpr uint32_t kSpriteRotates = 0x40;
constexpr uint32_t kSpriteHasMid = 0x800;
constexpr uint32_t kSpriteFacesMotion = 0x20000;
constexpr uint32_t kSpriteStartAspect = 0x40000;
constexpr uint32_t kSpriteMidAspect = 0x80000;
constexpr uint32_t kSpriteMidRelative = 0x4000000;
constexpr uint32_t kSpriteHasEnd = 0x20000000;
constexpr uint32_t kSpriteEndRelative = 0x80000000u;
constexpr uint32_t kSprite2EndAspect = 0x1;
constexpr uint32_t kSprite2MidOfEnd = 0x2;

constexpr uint32_t kShapeRound = 0x2;
constexpr uint32_t kShapeBox = 0x4;
constexpr uint32_t kShapeExactRadius = 0x8;
constexpr uint32_t kShapeCircle = 0x40;
constexpr uint32_t kShapePlaneXy = 0x80;
constexpr uint32_t kShapePlaneYz = 0x200;
constexpr uint32_t kShapeEven = 0x40000000;

constexpr uint32_t kMoveVelocity = 0x2;
constexpr uint32_t kMoveFlips[3] = { 0x8, 0x10, 0x20 };

typedef std::array<double, 3> Vector;
typedef std::array<double, 2> Pair;
typedef std::array<double, 4> Colour;

using BbtagParticle::Effect;

class Dice
{
public:
	Dice(std::initializer_list<uint32_t> keys)
	{
		uint32_t state = kFnvBasis;

		for (const uint32_t key : keys)
			state = (state ^ key) * kFnvPrime;

		m_state = state != 0 ? state : kDiceFallback;
	}

	uint32_t Next()
	{
		uint32_t x = m_state;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		m_state = x;

		return x;
	}

	double Unit() { return Next() / kDiceSpan; }

	double Between(double low, double high)
	{
		if (low == high)
			return low;

		return low + Unit() * (high - low);
	}

	double Between(const BbtagParticle::Range& range) { return Between(range.min, range.max); }

	int Count(int a, int b)
	{
		const int low = std::min(a, b);
		const int high = std::max(a, b);

		return low + std::min(static_cast<int>(Unit() * (high - low + 1)), high - low);
	}

private:
	uint32_t m_state;
};

int Rounded(double value)
{
	return static_cast<int>(std::floor(value + 0.5));
}

int CeilDiv(int value, int grid)
{
	return (value + grid - 1) / grid;
}

Colour Channels(uint32_t colour)
{
	return { static_cast<double>((colour >> 24) & 0xff), static_cast<double>((colour >> 16) & 0xff),
		static_cast<double>((colour >> 8) & 0xff), static_cast<double>(colour & 0xff) };
}

template <size_t N>
std::array<double, N> Blend(const std::array<double, N>& a, const std::array<double, N>& b, double t)
{
	std::array<double, N> out = {};

	for (size_t i = 0; i < N; ++i)
		out[i] = a[i] + (b[i] - a[i]) * t;

	return out;
}

template <size_t N>
std::array<double, N> KeyAt(const std::array<double, N>& start, const std::array<double, N>& mid,
	const std::array<double, N>& end, double midAt, double t)
{
	if (t < midAt)
		return Blend(start, mid, t / midAt);

	const double span = 1.0 - midAt;

	if (span <= 0.0)
		return end;

	return Blend(mid, end, (t - midAt) / span);
}

int Period(Dice& dice, int low, int high, int life)
{
	const int value = dice.Count(low, high);

	return value <= 0 ? life : value;
}

Vector SpherePoint(Dice& dice, double radius)
{
	while (true)
	{
		Vector point = {};

		for (double& c : point)
			c = dice.Between(-1.0, 1.0);

		const double length = std::sqrt(point[0] * point[0] + point[1] * point[1] + point[2] * point[2]);

		if (length > 0.0 && length <= 1.0)
			return { point[0] / length * radius, point[1] / length * radius, point[2] / length * radius };
	}
}

Vector CirclePoint(const BbtagParticle::Shape& shape, Dice& dice, int index, int count, double radius)
{
	double angle = shape.angleOffset + dice.Unit() * shape.angleRange;

	if ((shape.flags & kShapeEven) != 0 && count != 0)
		angle += kTau * index / count;

	const double a = std::cos(angle) * radius;
	const double b = std::sin(angle) * radius;

	if ((shape.flags & kShapePlaneXy) != 0)
		return { a, b, 0.0 };

	if ((shape.flags & kShapePlaneYz) != 0)
		return { 0.0, a, b };

	return { a, 0.0, b };
}

Vector BirthOffset(const BbtagParticle::Shape& shape, Dice& dice, const Pair& band, int index, int count)
{
	if ((shape.flags & kShapeBox) != 0)
	{
		const double x = dice.Between(shape.boxMin[0], shape.boxMax[0]);
		const double y = dice.Between(shape.boxMin[1], shape.boxMax[1]);

		return { x, y, dice.Between(band[0], band[1]) };
	}

	if ((shape.flags & kShapeRound) == 0)
		return { 0.0, 0.0, 0.0 };

	const double radius = (shape.flags & kShapeExactRadius) != 0 ? shape.radius : dice.Unit() * shape.radius;

	if ((shape.flags & kShapeCircle) != 0)
		return CirclePoint(shape, dice, index, count, radius);

	return SpherePoint(dice, radius);
}

struct Motion
{
	Vector velocity;
	Vector acceleration;
	double damping;
};

Motion BirthMotion(const BbtagParticle::Move& move, Dice& dice)
{
	Motion out = { { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, 1.0 };

	if ((move.flags & kMoveVelocity) == 0)
		return out;

	for (int axis = 0; axis < 3; ++axis)
		out.velocity[axis] = dice.Between(move.velocity[axis]);

	for (int axis = 0; axis < 3; ++axis)
	{
		if ((move.flags & kMoveFlips[axis]) != 0 && dice.Unit() < 0.5)
			out.velocity[axis] = -out.velocity[axis];
	}

	for (int axis = 0; axis < 3; ++axis)
		out.acceleration[axis] = dice.Between(move.accel[axis]);

	out.damping = move.damping;
	return out;
}

Pair KeyedSize(const BbtagParticle::Size& size, Dice& dice, bool aspect)
{
	const double widthMax = size.width.max;
	const double width = dice.Between(size.width.min, widthMax);

	if (aspect && widthMax != 0.0)
		return { width, width * size.height.max / widthMax };

	return { width, dice.Between(size.height) };
}

Pair Scaled(const Pair& a, const Pair& b)
{
	return { a[0] * b[0], a[1] * b[1] };
}

std::array<Pair, 3> SizeKeys(const BbtagParticle::Sprite& sprite, Dice& dice)
{
	const Pair start = KeyedSize(sprite.scaleStart, dice, (sprite.flags & kSpriteStartAspect) != 0);
	Pair end = start;
	Pair mid = start;

	if ((sprite.flags & kSpriteHasEnd) != 0)
	{
		end = KeyedSize(sprite.scaleEnd, dice, (sprite.flags2 & kSprite2EndAspect) != 0);

		if ((sprite.flags & kSpriteEndRelative) != 0)
			end = Scaled(end, start);
	}

	if ((sprite.flags & kSpriteHasMid) == 0)
		return { start, mid, end };

	mid = KeyedSize(sprite.scaleMid, dice, (sprite.flags & kSpriteMidAspect) != 0);

	if ((sprite.flags & kSpriteMidRelative) != 0)
		mid = Scaled(mid, start);

	if ((sprite.flags & kSpriteHasEnd) != 0 && (sprite.flags2 & kSprite2MidOfEnd) != 0)
		mid = Scaled(mid, end);

	return { start, mid, end };
}

struct Spin
{
	double angle;
	double speed;
	double accel;
	double damping;
};

Spin SpinKeys(const BbtagParticle::Sprite& sprite, Dice& dice)
{
	if ((sprite.flags & kSpriteRotates) == 0)
		return { 0.0, 0.0, 0.0, 1.0 };

	const double angle = dice.Between(sprite.rotationStart);
	const double speed = dice.Between(sprite.rotationSpeed);
	const double accel = dice.Between(sprite.rotationAccel);

	return { angle, speed, accel, sprite.rotationDamping };
}

void Knots(int length, int span, double midAt, std::set<int>& out)
{
	for (int start = 0; start < length; start += span)
	{
		out.insert(start);

		if (start > 0)
			out.insert(start - 1);

		const int mid = start + Rounded(midAt * span);

		if (mid < length)
			out.insert(mid);
	}
}

bool Curved(const Effect& effect)
{
	bool accelerates = false;

	for (const BbtagParticle::Range& range : effect.move.accel)
		accelerates = accelerates || range.min != 0.0 || range.max != 0.0;

	const BbtagParticle::Sprite& sprite = effect.sprite;
	const bool spinsUnevenly = (sprite.flags & kSpriteRotates) != 0 && (sprite.rotationAccel.min != 0.0
		|| sprite.rotationAccel.max != 0.0 || sprite.rotationDamping != 1.0);

	return accelerates || effect.move.damping != 1.0 || spinsUnevenly
		|| (sprite.flags & kSpriteFacesMotion) != 0;
}

struct State
{
	Vector position;
	Pair size;
	Colour colour;
	double angle;
};

int SunkAt(const std::vector<State>& states)
{
	for (size_t age = 0; age < states.size(); ++age)
	{
		if (states[age].position[1] < 0.0)
			return static_cast<int>(age);
	}

	return static_cast<int>(states.size());
}

class Life
{
public:
	Life(const Effect& effect, const Vector& origin, Dice& dice, int length, const Pair& band,
		int index, int count, int grid)
		: m_length(length), m_sunk(length)
	{
		const BbtagParticle::Sprite& sprite = effect.sprite;
		const double scale = effect.shape.sizeScale != 0.0 ? effect.shape.sizeScale : 1.0;
		const Vector offset = BirthOffset(effect.shape, dice, band, index, count);
		Vector position = { origin[0] + offset[0] * scale, origin[1] + offset[1] * scale,
			origin[2] + offset[2] * scale };
		Motion motion = BirthMotion(effect.move, dice);
		const Colour colours[3] = { Channels(sprite.colourStart), Channels(sprite.colourMid),
			Channels(sprite.colourEnd) };
		const int colourSpan = Period(dice, sprite.colourPeriodMin, sprite.colourPeriodMax, length);
		const std::array<Pair, 3> sizes = SizeKeys(sprite, dice);
		const int sizeSpan = Period(dice, sprite.scalePeriodMin, sprite.scalePeriodMax, length);
		Spin spin = SpinKeys(sprite, dice);

		m_states.reserve(static_cast<size_t>(length));

		for (int age = 0; age < length; ++age)
		{
			const Colour colour = KeyAt(colours[0], colours[1], colours[2], sprite.colourMidAt,
				(age % colourSpan) / static_cast<double>(colourSpan));
			const Pair size = KeyAt(sizes[0], sizes[1], sizes[2], sprite.scaleMidAt,
				(age % sizeSpan) / static_cast<double>(sizeSpan));

			m_states.push_back({ position, { size[0] * scale, size[1] * scale }, colour, spin.angle });

			for (int axis = 0; axis < 3; ++axis)
			{
				motion.velocity[axis] = motion.damping * (motion.velocity[axis] + motion.acceleration[axis]);
				position[axis] = position[axis] + motion.velocity[axis] * scale;
			}

			spin.speed = (spin.speed + spin.accel) * spin.damping;
			spin.angle += spin.speed;
		}

		if ((sprite.flags & kSpriteDepthTest) != 0)
			m_sunk = SunkAt(m_states);

		if (grid > 1)
		{
			for (int age = 0; age < length; age += grid)
				m_keys.push_back(age);

			return;
		}

		std::set<int> keys = { 0, length - 1 };
		Knots(length, colourSpan, sprite.colourMidAt, keys);

		if (sizes[0] != sizes[1] || sizes[0] != sizes[2])
			Knots(length, sizeSpan, sprite.scaleMidAt, keys);

		if (m_sunk < length)
		{
			keys.insert(m_sunk);
			keys.insert(std::max(m_sunk - 1, 0));
		}

		for (const int key : keys)
		{
			if (key >= 0 && key < length)
				m_keys.push_back(key);
		}
	}

	int Length() const { return m_length; }
	int Sunk() const { return m_sunk; }
	int LastKey() const { return m_keys.back(); }
	const std::vector<int>& Keys() const { return m_keys; }
	const State& At(int age) const { return m_states[static_cast<size_t>(age)]; }

private:
	int m_length;
	int m_sunk;
	std::vector<State> m_states;
	std::vector<int> m_keys;
};

struct Lived
{
	int birth;
	Life life;
};

typedef std::vector<Lived> Lane;

bool Respawns(const BbtagParticle::Group& group)
{
	return (group.flags & kGroupRespawns) != 0;
}

bool Staggered(const BbtagParticle::Group& group)
{
	return (group.flags & kGroupStaggered) == kGroupStaggered;
}

void LifeRange(const BbtagParticle::Group& group, int& low, int& high)
{
	if (group.lifeMax <= 0)
	{
		low = high = 1;
		return;
	}

	low = std::max(1, std::min(group.lifeMin, group.lifeMax));
	high = std::max(group.lifeMin, group.lifeMax);
}

void GapRange(const BbtagParticle::Group& group, int& low, int& high)
{
	if (Staggered(group))
	{
		low = high = 0;
		return;
	}

	low = std::max(0, std::min(group.delayMin, group.delayMax));
	high = std::max(0, std::max(group.delayMin, group.delayMax));
}

int OnGrid(double value, int grid, int low, int high)
{
	const int lowest = CeilDiv(low, grid) * grid;
	const int highest = std::max(lowest, high / grid * grid);

	return std::max(lowest, std::min(highest, Rounded(value / grid) * grid));
}

int LoopLength(const BbtagParticle::Group& group, int grid)
{
	int low = 0;
	int high = 0;
	int gapLow = 0;
	int gapHigh = 0;
	LifeRange(group, low, high);
	GapRange(group, gapLow, gapHigh);

	return kLives * OnGrid((low + high + gapLow + gapHigh) / 2.0, grid, grid, kUnbounded);
}

void Fitted(Dice& dice, const BbtagParticle::Group& group, int loop, int grid,
	std::vector<int>& lengths, std::vector<int>& gaps)
{
	int low = 0;
	int high = 0;
	int gapLow = 0;
	int gapHigh = 0;
	LifeRange(group, low, high);
	GapRange(group, gapLow, gapHigh);

	const int lowest = OnGrid(low, grid, low, high);
	const int highest = OnGrid(high, grid, low, high);
	lengths.assign(kLives, 0);
	gaps.assign(kLives, 0);

	for (int& length : lengths)
		length = OnGrid(dice.Count(low, high), grid, low, high);

	for (int& gap : gaps)
		gap = gapHigh != 0 ? OnGrid(dice.Count(gapLow, gapHigh), grid, 0, kUnbounded) : 0;

	int left = loop;

	for (int i = 0; i < kLives; ++i)
		left -= lengths[i] + gaps[i];

	while (left != 0)
	{
		bool moved = false;

		for (int i = 0; i < kLives && left != 0; ++i)
		{
			const int step = left > 0 ? grid : -grid;

			if (lengths[i] + step < lowest || lengths[i] + step > highest)
				continue;

			lengths[i] += step;
			left -= step;
			moved = true;
		}

		if (!moved)
			break;
	}

	if (left != 0)
	{
		gaps.back() = std::max(0, gaps.back() + left);
		left = loop;

		for (int i = 0; i < kLives; ++i)
			left -= lengths[i] + gaps[i];
	}

	if (left != 0)
		lengths.back() = std::max(grid, lengths.back() + left);
}

int Modulo(int value, int loop)
{
	return ((value % loop) + loop) % loop;
}

struct Plan
{
	const Effect* effect;
	std::vector<Vector> origins;
	std::vector<int> lanes;
	int cell;
};

struct Entry
{
	char letter;
	int index;
	int effect;
	int loop;
	double depth;
	int layerZ;
	int prio;
	std::vector<Lane> lanes;
};

Lane MakeLane(const Plan& plan, int effectIndex, int originIndex, int laneIndex, const Pair& band,
	int count, int loop, int grid)
{
	Dice dice({ kSeed, static_cast<uint32_t>(effectIndex), static_cast<uint32_t>(originIndex),
		kLaneDraw + static_cast<uint32_t>(laneIndex) });

	std::vector<int> lengths;
	std::vector<int> gaps;
	Fitted(dice, plan.effect->group, loop, grid, lengths, gaps);

	int birth = -dice.Count(0, loop / grid - 1) * grid;
	Lane out;

	for (size_t i = 0; i < lengths.size(); ++i)
	{
		const int start = Modulo(birth, loop);
		out.push_back({ start, Life(*plan.effect, plan.origins[static_cast<size_t>(originIndex)], dice,
			lengths[i], band, laneIndex, count, grid) });
		birth += lengths[i] + gaps[i];
	}

	return out;
}

bool VolumeBorn(const Effect& effect)
{
	return (effect.shape.flags & kShapeBox) != 0;
}

int Bands(const Plan& plan, int lanes, int perBand)
{
	return VolumeBorn(*plan.effect) ? CeilDiv(lanes, perBand) : 1;
}

int EntriesWanted(const std::vector<Plan>& plans, int perBand)
{
	int total = 0;

	for (const Plan& plan : plans)
	{
		for (const int lanes : plan.lanes)
			total += Bands(plan, lanes, perBand);
	}

	return total;
}

int LayerDepth(double z)
{
	return Rounded(z * kLayerFocal / BbtagCamera::kEyeDistance);
}

double EntryScale(int layerZ)
{
	return kLayerFocal / std::max(kLayerFocal + layerZ, kNearest);
}

void BuildEntries(const std::vector<Plan>& plans, std::vector<Entry>& out)
{
	int perBand = kLanesPerBand;

	while (EntriesWanted(plans, perBand) > kEntryLimit)
		++perBand;

	for (size_t e = 0; e < plans.size(); ++e)
	{
		const Plan& plan = plans[e];
		const BbtagParticle::Shape& shape = plan.effect->shape;
		const int grid = Curved(*plan.effect) ? kStep : 1;
		const int loop = LoopLength(plan.effect->group, grid);
		const double scale = shape.sizeScale != 0.0 ? shape.sizeScale : 1.0;
		const bool volume = VolumeBorn(*plan.effect);

		for (size_t o = 0; o < plan.origins.size(); ++o)
		{
			const int lanes = plan.lanes[o];
			const int bands = Bands(plan, lanes, perBand);
			const double low = volume ? shape.boxMin[2] : 0.0;
			const double high = volume ? shape.boxMax[2] : 0.0;
			const double width = (high - low) / bands;

			for (int band = 0; band < bands; ++band)
			{
				const Pair span = { low + band * width, low + (band + 1) * width };
				const double depth = plan.origins[o][2] + (span[0] + span[1]) * 0.5 * scale;

				Entry entry;
				entry.letter = static_cast<char>('a' + e);
				entry.index = static_cast<int>(out.size());
				entry.effect = static_cast<int>(e);
				entry.loop = loop;
				entry.depth = depth;
				entry.layerZ = LayerDepth(depth);
				entry.prio = depth >= 0.0 ? kPrioBehind : kPrioFront;

				for (int k = band * lanes / bands; k < (band + 1) * lanes / bands; ++k)
				{
					entry.lanes.push_back(MakeLane(plan, static_cast<int>(e), static_cast<int>(o), k, span,
						lanes, loop, grid));
				}

				out.push_back(std::move(entry));
			}
		}
	}
}

class Camera
{
public:
	Camera()
	{
		const double half = std::tan(BbtagCamera::kFov * BbtagCamera::kPi / 360.0);

		m_pixels = kFrameHeight / (2.0 * BbtagCamera::kEyeDistance * half);
		m_focal = m_pixels * BbtagCamera::kEyeDistance;
		m_ground = BbtagCamera::kEyeHeight * m_pixels;
	}

	double Factor(const Vector& position) const
	{
		return m_focal / std::max(BbtagCamera::kEyeDistance + position[2], kNearest);
	}

	Pair Project(const Vector& position) const
	{
		const double factor = Factor(position);

		return { position[0] * factor, -((position[1] - BbtagCamera::kEyeHeight) * factor + m_ground) };
	}

	double Ground() const { return m_ground; }

private:
	double m_pixels;
	double m_focal;
	double m_ground;
};

std::vector<double> Turns(const Life& life, const Camera& camera, bool facesMotion)
{
	std::vector<double> out;
	out.reserve(static_cast<size_t>(life.Length()));

	if (!facesMotion)
	{
		for (int age = 0; age < life.Length(); ++age)
			out.push_back(life.At(age).angle / kTau);

		return out;
	}

	bool known = false;
	double last = 0.0;

	for (int age = 0; age < life.Length(); ++age)
	{
		const int ahead = age + 1 < life.Length() ? age + 1 : age;
		const int behind = ahead - 1;
		double dx = 0.0;
		double dy = 0.0;

		if (behind >= 0)
		{
			const Pair to = camera.Project(life.At(ahead).position);
			const Pair from = camera.Project(life.At(behind).position);
			dx = to[0] - from[0];
			dy = to[1] - from[1];
		}

		double turn = (dx != 0.0 || dy != 0.0) ? std::atan2(dx, -dy) / kTau : last;

		if (known)
			turn -= std::floor(turn - last + 0.5);

		out.push_back(turn);
		last = turn;
		known = true;
	}

	return out;
}

struct Cell
{
	int part;
	std::string name;
	int source[4];
	int pretint[3];
	int x;
	int y;
};

void Pretint(const BbtagParticle::Sprite& sprite, int out[3])
{
	const Colour keys[3] = { Channels(sprite.colourStart), Channels(sprite.colourMid),
		Channels(sprite.colourEnd) };

	for (int channel = 0; channel < 3; ++channel)
	{
		const double peak = std::max(keys[0][channel + 1], std::max(keys[1][channel + 1], keys[2][channel + 1]));
		out[channel] = peak != 0.0 ? static_cast<int>(peak) : 255;
	}
}

PatWriter::Sprite SpriteFor(int ident, const Life& life, int age, double turn, const Entry& entry,
	const Cell& cell, const Camera& camera)
{
	const State& state = life.At(age);
	const double factor = camera.Factor(state.position);
	const Pair point = camera.Project(state.position);
	const double fit = EntryScale(entry.layerZ);
	const double centre = -camera.Ground();
	const bool visible = age < life.Sunk();

	PatWriter::Sprite out = {};
	out.id = ident;
	out.x = Rounded(point[0] / fit);
	out.y = Rounded(centre + (point[1] - centre) / fit);
	out.additive = kAdditive;
	out.zoom[0] = static_cast<float>(state.size[0] * factor / (cell.source[2] * fit));
	out.zoom[1] = static_cast<float>(state.size[1] * factor / (cell.source[3] * fit));
	out.priority = kSpritePriority;
	out.part = cell.part;
	out.tinted = true;
	out.tint[3] = static_cast<uint8_t>(visible ? std::max(0, std::min(255, Rounded(state.colour[0]))) : 0);

	for (int channel = 0; channel < 3; ++channel)
	{
		out.tint[channel] = static_cast<uint8_t>(std::min(255,
			Rounded(state.colour[channel + 1] * 255.0 / cell.pretint[channel])));
	}

	out.turned = true;
	out.turn = static_cast<float>(turn);
	return out;
}

PatWriter::Sprite HiddenSprite(const Cell& cell)
{
	PatWriter::Sprite out = {};
	out.additive = kAdditive;
	out.zoom[0] = kHiddenZoom;
	out.zoom[1] = kHiddenZoom;
	out.priority = kSpritePriority;
	out.part = cell.part;
	out.tinted = true;
	return out;
}

struct Frame
{
	PatWriter::Pattern pattern;
	int wait;
};

void EntryFrames(const Entry& entry, const Cell& cell, const Camera& camera, bool facesMotion,
	std::vector<Frame>& out)
{
	std::set<int> times = { 0 };
	std::map<const Life*, std::vector<double> > turned;

	for (const Lane& lane : entry.lanes)
	{
		for (const Lived& lived : lane)
		{
			for (const int age : lived.life.Keys())
				times.insert(Modulo(lived.birth + age, entry.loop));

			turned[&lived.life] = Turns(lived.life, camera, facesMotion);
		}
	}

	const std::vector<int> sorted(times.begin(), times.end());

	for (size_t k = 0; k < sorted.size(); ++k)
	{
		const int when = sorted[k];
		Frame frame;
		char name[32] = {};
		sprintf_s(name, "%c%02d_%04d", entry.letter, entry.index, static_cast<int>(k));
		frame.pattern.name = name;
		frame.wait = (k + 1 < sorted.size() ? sorted[k + 1] : entry.loop) - when;

		for (size_t number = 0; number < entry.lanes.size(); ++number)
		{
			const Lane& lane = entry.lanes[number];

			for (size_t which = 0; which < lane.size(); ++which)
			{
				const Life& life = lane[which].life;
				const int age = Modulo(when - lane[which].birth, entry.loop);

				if (age > life.LastKey())
					continue;

				const int ident = static_cast<int>(2 * number + which % 2);
				frame.pattern.sprites.push_back(SpriteFor(ident, life, age,
					turned[&life][static_cast<size_t>(age)], entry, cell, camera));
				break;
			}
		}

		if (frame.pattern.sprites.empty())
			frame.pattern.sprites.push_back(HiddenSprite(cell));

		out.push_back(std::move(frame));
	}
}

bool ShelfPack(const std::vector<Cell>& cells, int side, int grain, std::vector<std::array<int, 2> >& out)
{
	out.clear();

	int x = 0;
	int y = 0;
	int row = 0;

	for (const Cell& cell : cells)
	{
		const int width = cell.source[2];
		const int height = cell.source[3];

		if (x + width > side)
		{
			x = 0;
			y += row;
			row = 0;
		}

		if (y + height > side || width > side)
			return false;

		out.push_back({ x, y });
		x += CeilDiv(width, grain) * grain;
		row = std::max(row, CeilDiv(height, grain) * grain);
	}

	return true;
}

bool PackCells(std::vector<Cell>& cells, int& side)
{
	for (const int candidate : kAtlasSides)
	{
		std::vector<std::array<int, 2> > places;

		if (!ShelfPack(cells, candidate, candidate / kUvSpace, places))
			continue;

		for (size_t i = 0; i < cells.size(); ++i)
		{
			cells[i].x = places[i][0];
			cells[i].y = places[i][1];
		}

		side = candidate;
		return true;
	}

	return false;
}

PatWriter::Atlas AtlasFor(const std::vector<Cell>& cells, int side, const BbtagParticle::Surface& surface,
	const std::string& stage)
{
	PatWriter::Atlas atlas;
	atlas.name = stage;
	atlas.width = side;
	atlas.height = side;
	atlas.rgba.assign(static_cast<size_t>(side) * side * 4, 0);

	for (const Cell& cell : cells)
	{
		for (int yy = 0; yy < cell.source[3]; ++yy)
		{
			for (int xx = 0; xx < cell.source[2]; ++xx)
			{
				const size_t at = (static_cast<size_t>(cell.source[1] + yy) * surface.width + cell.source[0] + xx) * 4;
				const size_t to = (static_cast<size_t>(cell.y + yy) * side + cell.x + xx) * 4;

				if (at + 3 >= surface.bgra.size())
					continue;

				atlas.rgba[to] = static_cast<uint8_t>(surface.bgra[at + 2] * cell.pretint[0] / 255);
				atlas.rgba[to + 1] = static_cast<uint8_t>(surface.bgra[at + 1] * cell.pretint[1] / 255);
				atlas.rgba[to + 2] = static_cast<uint8_t>(surface.bgra[at] * cell.pretint[2] / 255);
				atlas.rgba[to + 3] = surface.bgra[at + 3];
			}
		}
	}

	return atlas;
}

std::vector<PatWriter::Cutout> Cutouts(const std::vector<Cell>& cells, int side)
{
	const int grain = side / kUvSpace;
	std::vector<PatWriter::Cutout> out;

	for (const Cell& cell : cells)
	{
		PatWriter::Cutout cut = {};
		cut.id = cell.part;
		cut.name = cell.name;
		cut.uv[0] = cell.x / grain;
		cut.uv[1] = cell.y / grain;
		cut.uv[2] = cell.source[2] / grain;
		cut.uv[3] = cell.source[3] / grain;
		cut.size[0] = cell.source[2];
		cut.size[1] = cell.source[3];
		cut.pivot[0] = cell.source[2] / 2;
		cut.pivot[1] = cell.source[3] / 2;
		out.push_back(cut);
	}

	return out;
}

std::string ObjectText(const std::string& stage, const std::string& patName,
	const std::vector<Entry>& entries, const std::vector<std::vector<Frame> >& frames)
{
	std::string out = "BgObject <-\r\n{\r\n\tpanidata = \"./bg/" + stage + "/" + patName + "\",\r\n\r\n";

	for (size_t i = 0; i < entries.size(); ++i)
	{
		char line[160] = {};
		sprintf_s(line, "\tdata%03d =\r\n\t[\r\n", static_cast<int>(i + 1));
		out += line;

		for (const Frame& frame : frames[i])
		{
			sprintf_s(line, "\t\t{ tag=\"frm\", name=\"%s\", wait=%d },\r\n", frame.pattern.name.c_str(),
				frame.wait);
			out += line;
		}

		sprintf_s(line, "\t\t{ tag=\"prio\", val=%d },\r\n\t\t{ tag=\"startpos\", x=0, y=0, z=%d },\r\n"
			"\t\t{ tag=\"startdelay\", val=0 },\r\n\t]\r\n\r\n", entries[i].prio, entries[i].layerZ);
		out += line;
	}

	return out + "}\r\n";
}

bool CellKey(const Cell& a, const Cell& b)
{
	return std::equal(a.source, a.source + 4, b.source) && std::equal(a.pretint, a.pretint + 3, b.pretint);
}

int CellFor(const Effect& effect, std::vector<Cell>& cells)
{
	Cell cell = {};
	const int x = static_cast<int>(effect.sprite.uv[0]);
	const int y = static_cast<int>(effect.sprite.uv[1]);
	cell.source[0] = x;
	cell.source[1] = y;
	cell.source[2] = static_cast<int>(effect.sprite.uv[2]) - x;
	cell.source[3] = static_cast<int>(effect.sprite.uv[3]) - y;
	Pretint(effect.sprite, cell.pretint);

	for (size_t i = 0; i < cells.size(); ++i)
	{
		if (CellKey(cells[i], cell))
			return static_cast<int>(i);
	}

	cell.part = static_cast<int>(cells.size()) + 1;
	cell.name = effect.name;
	cells.push_back(cell);

	return static_cast<int>(cells.size()) - 1;
}

}

bool BbtagParticleBake::Bake(const std::vector<BbtagParticle::Effect>& effects,
	const std::vector<Spawned>& spawned, const BbtagParticle::Surface& surface,
	const std::string& stage, Layer& out)
{
	out = Layer();

	std::vector<Plan> plans;
	std::vector<Cell> cells;

	for (const Spawned& one : spawned)
	{
		const Effect* const effect = BbtagParticle::Find(effects, one.effect);

		if (effect == nullptr || !Respawns(effect->group) || effect->sprite.sharedAtlas != 0)
			continue;

		Plan plan;
		plan.effect = effect;
		plan.origins.assign(one.origins.begin(), one.origins.end());
		plan.cell = CellFor(*effect, cells);

		const uint32_t effectIndex = static_cast<uint32_t>(plans.size());

		for (size_t o = 0; o < plan.origins.size(); ++o)
		{
			Dice dice({ kSeed, effectIndex, static_cast<uint32_t>(o), kCountDraw });
			plan.lanes.push_back(std::max(1, dice.Count(effect->group.countMin, effect->group.countMax)));
		}

		plans.push_back(plan);
	}

	int side = 0;

	if (plans.empty() || surface.bgra.empty() || !PackCells(cells, side))
		return false;

	std::vector<Entry> entries;
	BuildEntries(plans, entries);

	if (entries.empty() || static_cast<int>(entries.size()) > kEntryLimit)
		return false;

	const Camera camera;
	std::vector<std::vector<Frame> > frames(entries.size());
	std::vector<PatWriter::Pattern> patterns;

	for (size_t i = 0; i < entries.size(); ++i)
	{
		const Plan& plan = plans[static_cast<size_t>(entries[i].effect)];
		const bool facesMotion = (plan.effect->sprite.flags & kSpriteFacesMotion) != 0;
		EntryFrames(entries[i], cells[static_cast<size_t>(plan.cell)], camera, facesMotion, frames[i]);

		for (const Frame& frame : frames[i])
			patterns.push_back(frame.pattern);
	}

	out.patName = stage + kPatSuffix;
	out.pat = PatWriter::Build(patterns, Cutouts(cells, side), AtlasFor(cells, side, surface, stage));

	if (out.pat.size() > kPatLimit)
	{
		out = Layer();
		return false;
	}

	const std::string text = ObjectText(stage, out.patName, entries, frames);
	out.objects.assign(text.begin(), text.end());
	out.entries = static_cast<int>(entries.size());
	out.patterns = static_cast<int>(patterns.size());

	return true;
}
