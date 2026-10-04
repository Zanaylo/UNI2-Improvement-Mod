#include "Game/Stages/Mbaa/MbaaScene.h"

namespace {

constexpr int kSlots = 2000;
constexpr int kSubpixels = 128;
constexpr int kStarting = 1;
constexpr int kRunning = 2;
constexpr int kAxes = 2;
constexpr uint64_t kFnvBasis = 0xcbf29ce484222325ull;
constexpr uint64_t kFnvPrime = 0x100000001b3ull;

void Mix(uint64_t& hash, int value)
{
	hash = (hash ^ static_cast<uint32_t>(value)) * kFnvPrime;
}

int NextOf(const MbaaBg::Frame& frame, int current, int counter)
{
	switch (frame.op)
	{
	case MbaaBg::Op_Next:
	case MbaaBg::Op_NextToo:
		return current + 1;

	case MbaaBg::Op_Jump:
	case MbaaBg::Op_JumpToo:
		return frame.jump;

	case MbaaBg::Op_Count:
		return counter > 1 ? frame.jump : frame.exit;

	default:
		return current;
	}
}

}

MbaaScene::MbaaScene(const MbaaBg::File& file, uint32_t seed)
	: m_file(file)
	, m_actors(kSlots)
	, m_seed(seed == 0 ? 1 : seed)
{
	for (const MbaaBg::Layer& layer : file.layers)
	{
		if (layer.placed && layer.object >= 0 && layer.object < kSlots)
			Start(m_actors[static_cast<size_t>(layer.object)], layer);
	}
}

void MbaaScene::Start(Actor& actor, const MbaaBg::Layer& layer)
{
	actor = Actor();
	actor.layer = &layer;
	actor.state = kStarting;
	actor.instance = ++m_instances;
	Enter(actor);
}

void MbaaScene::Enter(Actor& actor)
{
	actor.timer = 0;
	actor.fired = false;
	actor.pushed = false;

	if (actor.frame < 0 || actor.frame >= static_cast<int>(actor.layer->frames.size()))
		return;

	const MbaaBg::Frame& frame = actor.layer->frames[static_cast<size_t>(actor.frame)];

	if (frame.loops != 0)
		actor.counter = frame.loops;

	actor.next = NextOf(frame, actor.frame, actor.counter);
}

void MbaaScene::Advance(Actor& actor)
{
	if (actor.state == kStarting)
		actor.state = kRunning;

	++actor.timer;

	const MbaaBg::Frame& frame = actor.layer->frames[static_cast<size_t>(actor.frame)];

	if (frame.duration > actor.timer)
		return;

	switch (frame.op)
	{
	case MbaaBg::Op_End:
		actor.state = 0;
		return;

	case MbaaBg::Op_Next:
	case MbaaBg::Op_NextToo:
		++actor.frame;
		break;

	case MbaaBg::Op_Jump:
	case MbaaBg::Op_JumpToo:
		actor.frame = frame.jump;
		actor.counter -= actor.counter > 0 ? 1 : 0;
		break;

	case MbaaBg::Op_Count:
		actor.counter -= actor.counter > 0 ? 1 : 0;
		actor.frame = actor.counter != 0 ? frame.jump : frame.exit;
		break;

	default:
		break;
	}

	if (actor.frame >= static_cast<int>(actor.layer->frames.size()))
	{
		actor.state = 0;
		return;
	}

	Enter(actor);
}

void MbaaScene::Move(Actor& actor)
{
	if (actor.pushed)
	{
		for (int axis = 0; axis < kAxes; ++axis)
		{
			actor.position[axis] += actor.velocity[axis];
			actor.velocity[axis] += actor.acceleration[axis];
		}

		return;
	}

	const MbaaBg::Frame& frame = actor.layer->frames[static_cast<size_t>(actor.frame)];
	const bool stop[kAxes] = { frame.stopX, frame.stopY };
	const bool push[kAxes] = { frame.pushX, frame.pushY };

	for (int axis = 0; axis < kAxes; ++axis)
	{
		if (stop[axis])
			actor.velocity[axis] = actor.acceleration[axis] = 0;

		if (!push[axis])
			continue;

		actor.velocity[axis] = frame.velocity[axis];
		actor.acceleration[axis] = frame.acceleration[axis];
	}

	actor.pushed = true;
}

int MbaaScene::Roll(int low, int high)
{
	m_rolled = true;

	if (high <= low)
		return low;

	m_seed ^= m_seed << 13;
	m_seed ^= m_seed >> 17;
	m_seed ^= m_seed << 5;

	return low + static_cast<int>(m_seed % static_cast<uint32_t>(high - low));
}

void MbaaScene::Spawn(const Actor& source, int object, int x, int y)
{
	const MbaaBg::Layer* const layer = MbaaBg::LayerOf(m_file, object);

	if (layer == nullptr || layer->frames.empty())
		return;

	for (Actor& actor : m_actors)
	{
		if (actor.state != 0)
			continue;

		Start(actor, *layer);

		const int share = source.layer->parallax;
		actor.position[0] = source.position[0] * share / MbaaBg::kFullParallax + x * kSubpixels;
		actor.position[1] = source.position[1] * share / MbaaBg::kFullParallax + y * kSubpixels;
		return;
	}
}

void MbaaScene::Velocity(Actor& actor, const MbaaBg::Record& event)
{
	if (event.target != 0)
		return;

	const int axis = event.values[4] != 0 ? 1 : 0;
	actor.velocity[axis] = Roll(event.values[0], event.values[1]);
	actor.acceleration[axis] = Roll(event.values[2], event.values[3]);
}

void MbaaScene::Fire(Actor& actor)
{
	if (actor.fired)
		return;

	actor.fired = true;

	const MbaaBg::Frame& frame = actor.layer->frames[static_cast<size_t>(actor.frame)];

	for (int index : frame.events)
	{
		if (index >= static_cast<int>(actor.layer->events.size()))
			continue;

		const MbaaBg::Record& event = actor.layer->events[static_cast<size_t>(index)];

		if (event.kind == MbaaBg::Event_Spawn)
			Spawn(actor, event.target, event.values[0], event.values[1]);
		else if (event.kind == MbaaBg::Event_SpawnAnywhere)
			Spawn(actor, event.target + Roll(0, event.values[4] & 0xffff),
				Roll(event.values[0], event.values[2] + 1), Roll(event.values[1], event.values[3] + 1));
		else if (event.kind == MbaaBg::Event_Velocity)
			Velocity(actor, event);
	}
}

void MbaaScene::Check(Actor& actor)
{
	const MbaaBg::Frame& frame = actor.layer->frames[static_cast<size_t>(actor.frame)];

	for (int index : frame.conditions)
	{
		if (index >= static_cast<int>(actor.layer->conditions.size()))
			continue;

		const MbaaBg::Record& condition = actor.layer->conditions[static_cast<size_t>(index)];

		if (condition.kind != MbaaBg::Condition_Position)
			continue;

		const int position = actor.position[condition.values[2] != 0 ? 1 : 0];
		const int threshold = condition.values[1];
		const bool crossed = condition.values[3] == 0 ? position > threshold : position < threshold;

		if (!crossed)
			continue;

		if (condition.values[0] < 0 || condition.values[0] >= static_cast<int>(actor.layer->frames.size()))
		{
			actor.state = 0;
			return;
		}

		actor.frame = condition.values[0];
		Enter(actor);
		return;
	}
}

void MbaaScene::Step()
{
	for (Actor& actor : m_actors)
	{
		if (actor.state != 0)
			Advance(actor);
	}

	for (Actor& actor : m_actors)
	{
		if (actor.state == kRunning)
			Move(actor);
	}

	for (size_t i = 0; i < m_actors.size(); ++i)
	{
		if (m_actors[i].state == kRunning)
			Fire(m_actors[i]);
	}

	for (Actor& actor : m_actors)
	{
		if (actor.state == kRunning)
			Check(actor);
	}
}

int MbaaScene::AlphaNow(const Actor& actor) const
{
	const std::vector<MbaaBg::Frame>& frames = actor.layer->frames;
	const MbaaBg::Frame& frame = frames[static_cast<size_t>(actor.frame)];
	const int alpha = MbaaBg::AlphaOf(frame);

	if (frame.tween == 0 || frame.duration == 0 || actor.next < 0 || actor.next >= static_cast<int>(frames.size()))
		return alpha;

	const int target = MbaaBg::AlphaOf(frames[static_cast<size_t>(actor.next)]);

	return alpha + (target - alpha) * actor.timer / frame.duration;
}

void MbaaScene::Visible(std::vector<Shown>& out) const
{
	out.clear();

	for (const Actor& actor : m_actors)
	{
		if (actor.state != kRunning)
			continue;

		const MbaaBg::Frame& frame = actor.layer->frames[static_cast<size_t>(actor.frame)];

		if (!MbaaBg::IsSprite(frame))
			continue;

		const int alpha = AlphaNow(actor);

		if (alpha <= 0)
			continue;

		Shown shown = {};
		shown.instance = actor.instance;
		shown.object = actor.layer->object;
		shown.image = frame.image;
		shown.x = frame.x + static_cast<float>(actor.position[0]) / kSubpixels;
		shown.y = frame.y + static_cast<float>(actor.position[1]) / kSubpixels;
		shown.blend = frame.blend;
		shown.alpha = alpha;

		out.push_back(shown);
	}
}

void MbaaScene::Living(std::vector<int>& out) const
{
	out.clear();

	for (const Actor& actor : m_actors)
	{
		if (actor.state != 0)
			out.push_back(actor.instance);
	}
}

uint64_t MbaaScene::Hash() const
{
	uint64_t hash = kFnvBasis;

	for (size_t i = 0; i < m_actors.size(); ++i)
	{
		const Actor& actor = m_actors[i];

		if (actor.state == 0)
			continue;

		Mix(hash, static_cast<int>(i));
		Mix(hash, actor.layer->object);
		Mix(hash, actor.state);
		Mix(hash, actor.frame);
		Mix(hash, actor.counter);
		Mix(hash, actor.timer);
		Mix(hash, actor.fired ? 1 : 0);
		Mix(hash, actor.pushed ? 1 : 0);

		for (int axis = 0; axis < kAxes; ++axis)
		{
			Mix(hash, actor.position[axis]);
			Mix(hash, actor.velocity[axis]);
			Mix(hash, actor.acceleration[axis]);
		}
	}

	return hash;
}
