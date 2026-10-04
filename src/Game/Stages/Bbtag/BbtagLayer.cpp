#include "Game/Stages/Bbtag/BbtagLayer.h"

#include "Game/Stages/Bbtag/BbtagCamera.h"
#include "Game/Stages/Bbtag/BbtagMua.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace {

constexpr double kTau = 2.0 * BbtagCamera::kPi;
constexpr float kUvSpace = 256.0f;
constexpr float kHiddenScale = 1e-4f;
constexpr float kBindableZoom = 1e-2f;
constexpr float kHalfTurn = 0.5f;
constexpr float kWholeTurn = 1.0f;
constexpr int kChannels = 4;
constexpr int kCorners = 4;
constexpr const char* kAtlasInfix = "_atlas";
constexpr const char* kAtlasSuffix = ".dds";

struct Key
{
	const PatReader::Pattern* pattern;
	int start;
	int wait;
};

struct Track
{
	int id;
	int part;
	int priority;
	int blend;
};

struct State
{
	float x;
	float y;
	float zoomX;
	float zoomY;
	float turns;
	float pitch;
	float yaw;
	float colour[kChannels];
};

std::vector<Key> Keys(const ObjectList::Entry& entry, const PatReader::Document& sheet,
	std::vector<std::string>& missing)
{
	std::vector<Key> out;
	int start = 0;

	for (const ObjectList::Frame& frame : entry.frames)
	{
		const PatReader::Pattern* const pattern = PatReader::Find(sheet, frame.name);

		if (pattern == nullptr)
		{
			if (std::find(missing.begin(), missing.end(), frame.name) == missing.end())
				missing.push_back(frame.name);

			continue;
		}

		const int wait = std::max(1, frame.wait);
		out.push_back(Key{ pattern, start, wait });
		start += wait;
	}

	return out;
}

bool Same(const PatReader::Sprite& sprite, const Track& track)
{
	return sprite.id == track.id && sprite.part == track.part;
}

const PatReader::Sprite* SpriteIn(const PatReader::Pattern& pattern, const Track& track)
{
	for (const PatReader::Sprite& sprite : pattern.sprites)
	{
		if (Same(sprite, track))
			return &sprite;
	}

	return nullptr;
}

std::vector<Track> Tracks(const std::vector<Key>& keys)
{
	std::vector<Track> out;

	for (const Key& key : keys)
	{
		for (const PatReader::Sprite& sprite : key.pattern->sprites)
		{
			const bool known = std::any_of(out.begin(), out.end(),
				[&sprite](const Track& track) { return Same(sprite, track); });

			if (!known)
				out.push_back(Track{ sprite.id, sprite.part, sprite.priority, sprite.blend });
		}
	}

	std::stable_sort(out.begin(), out.end(),
		[](const Track& left, const Track& right) { return left.priority < right.priority; });

	return out;
}

State StateOf(const PatReader::Sprite& sprite)
{
	State out = {};
	out.x = static_cast<float>(sprite.x);
	out.y = static_cast<float>(sprite.y);
	out.zoomX = sprite.zoomX;
	out.zoomY = sprite.zoomY;
	out.turns = sprite.turns;
	out.pitch = sprite.pitch;
	out.yaw = sprite.yaw;
	out.colour[0] = static_cast<float>((sprite.tint >> 16) & 0xff);
	out.colour[1] = static_cast<float>((sprite.tint >> 8) & 0xff);
	out.colour[2] = static_cast<float>(sprite.tint & 0xff);
	out.colour[3] = static_cast<float>(sprite.tint >> 24);

	return out;
}

float Mix(float from, float to, float t)
{
	return from + (to - from) * t;
}

float ShortestTurn(float from, float to)
{
	const float step = to - from;

	if (step > kHalfTurn)
		return step - kWholeTurn;

	if (step <= -kHalfTurn)
		return step + kWholeTurn;

	return step;
}

float MixTurns(float from, float to, float t)
{
	return from + ShortestTurn(from, to) * t;
}

State Between(const State& from, const State& to, float t)
{
	State out = {};
	out.x = Mix(from.x, to.x, t);
	out.y = Mix(from.y, to.y, t);
	out.zoomX = Mix(from.zoomX, to.zoomX, t);
	out.zoomY = Mix(from.zoomY, to.zoomY, t);
	out.turns = MixTurns(from.turns, to.turns, t);
	out.pitch = MixTurns(from.pitch, to.pitch, t);
	out.yaw = MixTurns(from.yaw, to.yaw, t);

	for (int c = 0; c < kChannels; ++c)
		out.colour[c] = Mix(from.colour[c], to.colour[c], t);

	return out;
}

size_t KeyAt(const std::vector<Key>& keys, int time)
{
	for (size_t i = 0; i < keys.size(); ++i)
	{
		if (time < keys[i].start + keys[i].wait)
			return i;
	}

	return keys.size() - 1;
}

bool StateAt(const std::vector<Key>& keys, const Track& track, int time, State& out)
{
	const size_t index = KeyAt(keys, time);
	const Key& key = keys[index];
	const PatReader::Sprite* const now = SpriteIn(*key.pattern, track);

	if (now == nullptr)
		return false;

	out = StateOf(*now);

	const PatReader::Sprite* const next = SpriteIn(*keys[(index + 1) % keys.size()].pattern, track);

	if (next != nullptr)
		out = Between(out, StateOf(*next), static_cast<float>(time - key.start) / static_cast<float>(key.wait));

	return true;
}

void Turn(int first, int second, double angle, float out[16])
{
	BbtagMua::Identity(out);

	const float cosine = static_cast<float>(cos(angle));
	const float sine = static_cast<float>(sin(angle));

	out[first * 4 + first] = cosine;
	out[first * 4 + second] = sine;
	out[second * 4 + first] = -sine;
	out[second * 4 + second] = cosine;
}

void Then(float matrix[16], const float next[16])
{
	float product[16] = {};
	BbtagMua::Multiply(matrix, next, product);
	memcpy(matrix, product, sizeof(product));
}

float Guarded(float zoom)
{
	if (fabsf(zoom) >= kHiddenScale)
		return zoom;

	return zoom < 0.0f ? -kHiddenScale : kHiddenScale;
}

State Bindable(const State& state)
{
	State out = state;
	out.zoomX = fabsf(state.zoomX) < kBindableZoom ? 1.0f : state.zoomX;
	out.zoomY = fabsf(state.zoomY) < kBindableZoom ? 1.0f : state.zoomY;

	return out;
}

BbtagPose::Pose JointPose(const State& state)
{
	float matrix[16] = {};
	BbtagMua::Identity(matrix);
	matrix[0] = Guarded(state.zoomX);
	matrix[5] = Guarded(state.zoomY);

	float step[16] = {};
	Turn(0, 1, state.turns * kTau, step);
	Then(matrix, step);
	Turn(1, 2, -state.pitch * kTau, step);
	Then(matrix, step);
	Turn(2, 0, -state.yaw * kTau, step);
	Then(matrix, step);

	matrix[12] = state.x;
	matrix[13] = state.y;

	BbtagPose::Pose out = {};
	BbtagPose::Split(matrix, out);

	return out;
}

BbtagPose::Pose FramePose(const float start[3])
{
	const float lateral = static_cast<float>(1.0 / BbtagCamera::LayerUnits());
	const float depth = static_cast<float>(BbtagCamera::kEyeDistance / BbtagCamera::kLayerFocal);

	float matrix[16] = {};
	BbtagMua::Identity(matrix);
	matrix[0] = lateral;
	matrix[5] = -lateral;
	matrix[10] = -depth;
	matrix[12] = start[0] * lateral;
	matrix[13] = -start[1] * lateral;
	matrix[14] = start[2] * depth;

	BbtagPose::Pose out = {};
	BbtagPose::Split(matrix, out);

	return out;
}

void Corners(const PatReader::Part& part, BbtagLayer::Corner out[kCorners])
{
	const float left = static_cast<float>(-part.pivotX);
	const float top = static_cast<float>(-part.pivotY);
	const float right = left + static_cast<float>(part.width);
	const float bottom = top + static_cast<float>(part.height);
	const float u0 = static_cast<float>(part.u) / kUvSpace;
	const float v0 = static_cast<float>(part.v) / kUvSpace;
	const float u1 = static_cast<float>(part.u + part.w) / kUvSpace;
	const float v1 = static_cast<float>(part.v + part.h) / kUvSpace;

	out[0] = BbtagLayer::Corner{ { left, top, 0.0f }, { u0, v0 } };
	out[1] = BbtagLayer::Corner{ { right, top, 0.0f }, { u1, v0 } };
	out[2] = BbtagLayer::Corner{ { right, bottom, 0.0f }, { u1, v1 } };
	out[3] = BbtagLayer::Corner{ { left, bottom, 0.0f }, { u0, v1 } };
}

bool SamePose(const BbtagPose::Pose& left, const BbtagPose::Pose& right)
{
	return memcmp(left.translation, right.translation, sizeof(left.translation)) == 0
		&& memcmp(left.turn, right.turn, sizeof(left.turn)) == 0
		&& memcmp(left.scale, right.scale, sizeof(left.scale)) == 0;
}

float Dot(const float left[4], const float right[4])
{
	return left[0] * right[0] + left[1] * right[1] + left[2] * right[2] + left[3] * right[3];
}

class Converter
{
public:
	Converter(const PatReader::Document& sheet, const std::string& stem, BbtagLayer::Layer& out)
		: m_sheet(sheet), m_stem(stem), m_out(out)
	{
	}

	void Add(const ObjectList::Entry& entry)
	{
		const std::vector<Key> keys = Keys(entry, m_sheet, m_out.missing);

		if (keys.empty())
			return;

		const int span = keys.back().start + keys.back().wait;
		const std::vector<Track> tracks = Tracks(keys);
		std::vector<int> blends;

		for (const Track& track : tracks)
		{
			if (std::find(blends.begin(), blends.end(), track.blend) == blends.end())
				blends.push_back(track.blend);
		}

		for (int blend : blends)
			AddGroup(entry, keys, tracks, span, blend);
	}

private:
	void AddGroup(const ObjectList::Entry& entry, const std::vector<Key>& keys, const std::vector<Track>& tracks,
		int span, int blend)
	{
		BbtagLayer::Group group = {};
		group.entry = entry.number;
		group.prio = entry.prio;
		group.blend = blend;
		group.span = span;
		group.frame = FramePose(entry.start);

		for (const Track& track : tracks)
		{
			BbtagLayer::Sprite sprite = {};

			if (track.blend != blend || !Describe(track, sprite))
				continue;

			Animate(keys, track, span, entry.delay, sprite);
			group.moves = group.moves || Moves(sprite);
			group.sprites.push_back(std::move(sprite));
		}

		if (group.sprites.empty())
			return;

		const int count = static_cast<int>(group.sprites.size());
		m_out.sprites += count;
		m_out.front += group.prio >= BbtagLayer::kFrontPrio ? count : 0;
		m_out.groups.push_back(std::move(group));
	}

	bool Describe(const Track& track, BbtagLayer::Sprite& out)
	{
		PatReader::Part part = {};

		if (!PatReader::PartOf(m_sheet, track.part, part))
			return false;

		out.atlas = AtlasIndex(part.atlas);

		if (out.atlas < 0)
			return false;

		Corners(part, out.corners);

		return true;
	}

	int AtlasIndex(int id)
	{
		const std::map<int, int>::const_iterator known = m_atlases.find(id);

		if (known != m_atlases.end())
			return known->second;

		const PatReader::Atlas* const atlas = PatReader::AtlasOf(m_sheet, id);

		if (atlas == nullptr)
			return -1;

		BbtagLayer::Image image;
		image.name = m_stem + kAtlasInfix + std::to_string(id) + kAtlasSuffix;
		image.data.assign(atlas->dds, atlas->dds + atlas->ddsSize);

		const int index = static_cast<int>(m_out.atlases.size());
		m_out.atlases.push_back(std::move(image));
		m_atlases[id] = index;

		return index;
	}

	static void Animate(const std::vector<Key>& keys, const Track& track, int span, int delay,
		BbtagLayer::Sprite& out)
	{
		const State reference = StateOf(*SpriteIn(*KeyWith(keys, track).pattern, track));
		out.rest = JointPose(Bindable(reference));
		double colour[kChannels] = {};
		int shown = 0;

		for (int frame = 0; frame < span; ++frame)
		{
			const int time = ((frame - delay) % span + span) % span;
			State state = {};
			const bool visible = StateAt(keys, track, time, state);
			BbtagPose::Pose pose = JointPose(visible ? state : reference);

			if (visible)
			{
				for (int c = 0; c < kChannels; ++c)
					colour[c] += state.colour[c];

				++shown;
			}
			else
			{
				pose.scale[0] = pose.scale[1] = pose.scale[2] = kHiddenScale;
			}

			if (!out.poses.empty() && Dot(out.poses.back().turn, pose.turn) < 0.0f)
			{
				for (float& value : pose.turn)
					value = -value;
			}

			out.poses.push_back(pose);
		}

		out.poses.push_back(out.poses.front());

		for (int c = 0; c < kChannels; ++c)
			out.colour[c] = static_cast<uint8_t>(lround(shown > 0 ? colour[c] / shown : 0.0));
	}

	static const Key& KeyWith(const std::vector<Key>& keys, const Track& track)
	{
		for (const Key& key : keys)
		{
			if (SpriteIn(*key.pattern, track) != nullptr)
				return key;
		}

		return keys.front();
	}

	static bool Moves(const BbtagLayer::Sprite& sprite)
	{
		return std::any_of(sprite.poses.begin(), sprite.poses.end(),
			[&sprite](const BbtagPose::Pose& pose) { return !SamePose(pose, sprite.poses.front()); });
	}

	const PatReader::Document& m_sheet;
	const std::string& m_stem;
	BbtagLayer::Layer& m_out;
	std::map<int, int> m_atlases;
};

}

bool BbtagLayer::Convert(const std::vector<ObjectList::Entry>& entries, const PatReader::Document& sheet,
	const std::string& stem, Layer& out)
{
	out = Layer();

	Converter converter(sheet, stem, out);

	for (const ObjectList::Entry& entry : entries)
		converter.Add(entry);

	return !out.groups.empty();
}
