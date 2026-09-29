#include "Game/Stages/Bbtag/BbtagScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <tuple>

#include <string.h>

namespace {

constexpr size_t kHeader = 0x20;
constexpr size_t kRecord = 0x20;
constexpr size_t kBlocks = 0x30;
constexpr int kSheetBlock = 0;
constexpr int kNameBlock = 1;
constexpr int kParticleBlock = 2;

constexpr uint32_t kNone = 0xffffffffu;
constexpr uint32_t kYield = 0x02;
constexpr uint32_t kTime = 0x03;
constexpr uint32_t kClose = 0x04;
constexpr uint32_t kGroup = 0x05;
constexpr uint32_t kMotion = 0x06;
constexpr uint32_t kSpawn = 0x07;
constexpr uint32_t kBegin = 0x09;
constexpr uint32_t kEnd = 0x0a;
constexpr uint32_t kRect = 0x0b;
constexpr uint32_t kJump = 0x0f;
constexpr uint32_t kRamp = 0x12;
constexpr uint32_t kRandom = 0x13;
constexpr uint32_t kRandomEnd = 0x14;
constexpr uint32_t kPause = 0x15;
constexpr uint32_t kSceneOpen = 0x1a;
constexpr uint32_t kSceneClose = 0x1b;
constexpr uint32_t kSceneTilt = 0x28;

constexpr int kFree = 0;
constexpr int kSkipped = 1;
constexpr int kTaken = 2;
constexpr int kDone = 3;

constexpr int kFrames = 12000;
constexpr int kRolledFrames = 3600;
constexpr int kRollTries = 16;
constexpr size_t kPortRolledFrames = 600;
constexpr size_t kLongRun = 600;
constexpr size_t kSpriteBudget = 24000;
constexpr double kFull = 1000.0;
constexpr double kLampBend = 1e-3;
constexpr size_t kLampRamps = 512;
constexpr uint32_t kLcgMultiply = 1103515245u;
constexpr uint32_t kLcgAdd = 12345u;

typedef std::array<uint32_t, 8> Record;

uint32_t Dword(const std::vector<uint8_t>& blob, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, blob.data() + at, 4);

	return value;
}

uint16_t Word(const std::vector<uint8_t>& blob, size_t at)
{
	uint16_t value = 0;
	memcpy(&value, blob.data() + at, 2);

	return value;
}

void Block(const std::vector<uint8_t>& blob, int block, std::vector<std::string>& out)
{
	out.clear();

	const size_t stride = Dword(blob, 0x14);
	const size_t count = Word(blob, kHeader + block * 2);

	if (stride == 0)
		return;

	size_t at = kBlocks;

	for (int i = 0; i < block; ++i)
		at += Word(blob, kHeader + i * 2) * stride;

	for (size_t i = 0; i < count && at + stride <= blob.size(); ++i, at += stride)
	{
		std::string one;

		for (size_t k = 0; k < stride && blob[at + k] != 0; ++k)
		{
			const uint8_t letter = blob[at + k];

			if (letter < 0x20 || letter >= 0x7f)
				return;

			one.push_back(static_cast<char>(letter));
		}

		out.push_back(one);
	}
}

int64_t Signed(uint32_t value)
{
	return static_cast<int64_t>(static_cast<int32_t>(value));
}

uint32_t Crc32(const std::string& text)
{
	uint32_t crc = 0xffffffffu;

	for (const char letter : text)
	{
		crc ^= static_cast<uint8_t>(letter);

		for (int bit = 0; bit < 8; ++bit)
			crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
	}

	return ~crc;
}

bool SameRect(const BbtagScript::Rect& one, const BbtagScript::Rect& other)
{
	return one.sheet == other.sheet && one.x == other.x && one.y == other.y && one.w == other.w
		&& one.h == other.h;
}

bool SameLook(const BbtagScript::Sample& one, const BbtagScript::Sample& other)
{
	if (one.lit != other.lit || one.ramp != other.ramp)
		return false;

	return !one.lit || SameRect(one.rect, other.rect);
}

bool Same(const BbtagScript::Sample& one, const BbtagScript::Sample& other)
{
	if (one.lit != other.lit || one.ramp != other.ramp || one.take != other.take)
		return false;

	if (!one.lit)
		return true;

	return SameRect(one.rect, other.rect);
}

typedef std::tuple<int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, bool,
	std::array<uint32_t, 5>, double, double, int64_t, double, int64_t> State;

class Instance
{
public:
	Instance(const std::vector<Record>& record, const std::string& label)
		: m_record(record), m_dice(Crc32(label))
	{
		for (size_t at = 0; at < m_record.size(); ++at)
		{
			if (m_record[at][0] == kBegin && m_labels.find(m_record[at][1]) == m_labels.end())
				m_labels[m_record[at][1]] = static_cast<int64_t>(at);
		}
	}

	bool Rolled() const
	{
		return m_rolled;
	}

	void Play(std::vector<BbtagScript::Sample>& samples, bool& cyclic, int& from)
	{
		Interpret();
		Walk();

		std::map<State, int> seen;
		samples.clear();
		cyclic = false;
		from = 0;

		for (int tick = 0; tick < kFrames; ++tick)
		{
			if (tick != 0)
				Tick();

			samples.push_back(Current());

			if (m_rolled && !Ended())
			{
				if (tick + 1 >= kRolledFrames)
				{
					cyclic = true;
					return;
				}

				continue;
			}

			const State state = Snapshot();
			const std::map<State, int>::const_iterator known = seen.find(state);

			if (known != seen.end())
			{
				Repeating(samples, known->second, tick, cyclic, from);
				return;
			}

			seen[state] = tick;
		}

		from = static_cast<int>(samples.size()) - 1;
	}

private:
	BbtagScript::Sample Current() const
	{
		BbtagScript::Sample sample = {};
		sample.take = m_take;
		sample.since = m_since;

		if (m_ramp <= 0.0)
			return sample;

		sample.lit = m_lit;
		sample.ramp = m_ramp;

		if (m_lit)
			sample.rect = { static_cast<int>(m_rect[0]), static_cast<int>(m_rect[1]),
				static_cast<int>(m_rect[2]), static_cast<int>(m_rect[3]), static_cast<int>(m_rect[4]) };

		return sample;
	}

	bool Ended() const
	{
		if (m_jump >= 0)
			return false;

		if (m_resume >= static_cast<int64_t>(m_record.size()))
			return true;

		const uint32_t code = m_record[static_cast<size_t>(m_resume)][0];

		return code == kYield || code == kNone;
	}

	State Snapshot() const
	{
		return State(Ended() ? -1 : m_frame, m_jump, m_pause, m_resume, m_label, m_cursor, m_held,
			m_lit, m_lit ? m_rect : std::array<uint32_t, 5>(), m_ramp, m_target, m_left, m_step, m_take);
	}

	void Tick()
	{
		if (m_pause >= 1)
		{
			--m_pause;
		}
		else if (m_jump < 0)
		{
			++m_frame;
		}
		else
		{
			m_frame = m_jump;
			m_jump = -1;
			m_resume = 0;
		}

		if (m_label >= 0)
			++m_held;

		++m_since;

		if (m_left < 1)
		{
			m_ramp = m_target;
		}
		else
		{
			m_ramp += m_step;
			--m_left;
		}

		m_ramp = std::min(std::max(m_ramp, 0.0), kFull);

		if (m_pause < 1)
			Interpret();

		Walk();
	}

	void Interpret()
	{
		int64_t at = m_resume;
		bool live = false;
		int64_t opened = 0;
		int64_t chosen = -1;
		int rolling = kFree;

		for (; at < static_cast<int64_t>(m_record.size()); ++at)
		{
			const Record& fields = m_record[static_cast<size_t>(at)];
			const uint32_t code = fields[0];

			if (code == kYield || code == kNone)
				break;

			if (code == kTime && m_frame < static_cast<int64_t>(fields[1]))
				break;

			if (code == kTime)
			{
				if (m_frame == static_cast<int64_t>(fields[1]))
				{
					live = true;
					opened = fields[1];
				}
			}
			else if (code == kClose)
			{
				live = false;
			}
			else if (code == kRandom)
			{
				rolling = Roll(rolling, fields[1]);
			}
			else if (code == kRandomEnd)
			{
				rolling = kFree;
			}
			else if (live && (rolling == kFree || rolling == kTaken))
			{
				chosen = Command(fields, opened, chosen);
			}
		}

		m_resume = at;

		const std::map<int64_t, int64_t>::const_iterator label = m_labels.find(chosen);

		if (label == m_labels.end())
			return;

		m_label = chosen;
		m_cursor = label->second;
	}

	int Roll(int rolling, uint32_t percent)
	{
		if (rolling == kTaken || rolling == kDone)
			return kDone;

		m_rolled = true;
		m_dice = m_dice * kLcgMultiply + kLcgAdd;

		return ((m_dice >> 16) & 0x7fffu) % 100u < percent ? kTaken : kSkipped;
	}

	int64_t Command(const Record& fields, int64_t opened, int64_t chosen)
	{
		const uint32_t code = fields[0];

		if (code == kGroup)
		{
			m_held = m_frame - opened;
			return fields[1];
		}

		if (code == kMotion)
		{
			m_take = Signed(fields[1]);
			m_since = 0;
		}
		else if (code == kJump)
		{
			m_jump = Signed(fields[1]);
		}
		else if (code == kRamp)
		{
			m_target = static_cast<double>(fields[1]);
			m_left = Signed(fields[2]) == 0 ? 1 : Signed(fields[2]);
			m_step = (m_target - m_ramp) / static_cast<double>(m_left);
		}
		else if (code == kPause)
		{
			m_pause = Signed(fields[1]);
		}

		return chosen;
	}

	void Walk()
	{
		if (m_label < 0)
			return;

		const int64_t start = m_held;
		int64_t at = m_cursor;
		const int64_t count = static_cast<int64_t>(m_record.size());

		for (int64_t guard = 0; guard < 2 * count; ++guard, ++at)
		{
			if (at < 0 || at >= count || m_record[static_cast<size_t>(at)][0] == kNone)
				return;

			const Record& fields = m_record[static_cast<size_t>(at)];

			if (fields[0] == kRect && m_held < static_cast<int64_t>(fields[1]))
			{
				m_lit = true;
				std::copy(fields.begin() + 2, fields.begin() + 7, m_rect.begin());
				m_cursor = at;
				return;
			}

			if (fields[0] == kRect)
			{
				m_held -= fields[1];
				continue;
			}

			if (fields[0] != kEnd)
				continue;

			const int64_t span = start - m_held;

			if (span <= 0 || span == m_held)
				return;

			if (span < m_held)
				m_held %= span;

			at = m_labels.at(m_label);
		}
	}

	static void Repeating(std::vector<BbtagScript::Sample>& samples, int first, int now, bool& cyclic,
		int& from)
	{
		const int period = now - first;
		int start = first + 1;

		while (start > 0 && Same(samples[start - 1], samples[start - 1 + period]))
			--start;

		from = 0;

		if (start == 0)
		{
			samples.resize(period);
			cyclic = true;
			return;
		}

		if (period == 1)
		{
			samples.resize(start + 1);
			cyclic = false;
			from = start;
			return;
		}

		if (start <= period)
		{
			std::vector<BbtagScript::Sample> steady;

			for (int tick = 0; tick < period; ++tick)
			{
				const int lift = tick < start ? (start - tick + period - 1) / period : 0;
				steady.push_back(samples[tick + period * lift]);
			}

			samples.swap(steady);
			cyclic = true;
			return;
		}

		samples.resize(start + period);
		cyclic = false;
		from = start;
	}

	std::vector<Record> m_record;
	std::map<int64_t, int64_t> m_labels;
	uint32_t m_dice;
	int64_t m_frame = 0;
	int64_t m_jump = -1;
	int64_t m_pause = 0;
	int64_t m_resume = 0;
	int64_t m_label = -1;
	int64_t m_cursor = -1;
	int64_t m_held = 0;
	bool m_lit = false;
	std::array<uint32_t, 5> m_rect = {};
	double m_ramp = kFull;
	double m_target = kFull;
	int64_t m_left = 0;
	double m_step = 0.0;
	int64_t m_take = -1;
	int64_t m_since = 0;
	bool m_rolled = false;
};

bool Shows(const std::vector<BbtagScript::Sample>& samples)
{
	for (size_t i = samples.size() > 1 ? 1 : 0; i < samples.size(); ++i)
	{
		if (samples[i].ramp > 0.0)
			return true;
	}

	return false;
}

void Collect(const BbtagScript::Played& played, size_t begin, size_t count, BbtagScript::Sprite& out)
{
	out = BbtagScript::Sprite();
	out.sheets = played.sheets;

	for (size_t tick = begin; tick < begin + count && tick < played.sample.size(); ++tick)
	{
		const BbtagScript::Sample& sample = played.sample[tick];

		if (!sample.lit)
		{
			out.frame.push_back(-1);
			continue;
		}

		int index = -1;

		for (size_t i = 0; i < out.rect.size() && index < 0; ++i)
		{
			if (SameRect(out.rect[i], sample.rect))
				index = static_cast<int>(i);
		}

		if (index < 0)
		{
			index = static_cast<int>(out.rect.size());
			out.rect.push_back(sample.rect);
		}

		out.frame.push_back(index);
	}

	out.loop = static_cast<int>(out.frame.size());
}

bool Timeline(const std::vector<double>& level, int from, BbtagScript::Lamp& out)
{
	out = BbtagScript::Lamp();

	if (level.size() < 2)
		return false;

	if (std::all_of(level.begin(), level.end(), [&](double one) { return one == level[0]; }))
		return false;

	const size_t wrap = from >= 0 && static_cast<size_t>(from) < level.size()
		? static_cast<size_t>(from) : 0;

	std::vector<int> frames(1, 0);
	std::vector<double> kept(1, level[0]);

	for (size_t tick = 1; tick + 1 < level.size(); ++tick)
	{
		const double bend = (level[tick] - level[tick - 1]) - (level[tick + 1] - level[tick]);

		if (std::fabs(bend) > kLampBend)
		{
			frames.push_back(static_cast<int>(tick));
			kept.push_back(level[tick]);
		}
	}

	frames.push_back(static_cast<int>(level.size()) - 1);
	kept.push_back(level.back());
	frames.push_back(static_cast<int>(level.size()));
	kept.push_back(level[wrap]);

	const int first = static_cast<int>(std::floor(level[0] + 0.5));

	if (first != static_cast<int>(std::floor(level[wrap] + 0.5)))
		out.ramp.push_back({ 0, first, 0 });

	for (size_t i = 0; i + 1 < frames.size(); ++i)
	{
		BbtagScript::Ramp ramp = {};
		ramp.at = frames[i];
		ramp.target = static_cast<int>(std::floor(kept[i + 1] + 0.5));
		ramp.frames = frames[i + 1] - frames[i];
		out.ramp.push_back(ramp);
	}

	if (out.ramp.size() > kLampRamps)
	{
		out = BbtagScript::Lamp();
		return false;
	}

	out.loop = static_cast<int>(level.size());
	out.from = static_cast<int>(wrap);

	return true;
}

bool EndsWith(const std::string& text, const char* tail)
{
	const size_t size = strlen(tail);

	return text.size() >= size && _stricmp(text.c_str() + text.size() - size, tail) == 0;
}

}

bool BbtagScript::Play(const std::vector<uint8_t>& blob, const std::string& label, Played& out)
{
	out = Played();

	if (blob.size() < kBlocks || memcmp(blob.data(), "EVT0", 4) != 0)
		return false;

	Block(blob, kSheetBlock, out.sheets);
	Block(blob, kNameBlock, out.named);

	std::vector<Record> record;

	for (size_t at = Dword(blob, 0x10); at + kRecord <= blob.size(); at += kRecord)
	{
		Record fields = {};
		memcpy(fields.data(), blob.data() + at, kRecord);
		record.push_back(fields);

		if (fields[0] == kNone)
			break;
	}

	for (int attempt = 0; attempt < kRollTries; ++attempt)
	{
		Instance instance(record, attempt == 0 ? label : label + " " + std::to_string(attempt));
		instance.Play(out.sample, out.cyclic, out.from);
		out.rolled = instance.Rolled();

		if (Shows(out.sample) || !out.rolled)
			break;
	}

	return true;
}

size_t FirstLit(const BbtagScript::Played& played)
{
	for (size_t tick = 0; tick < played.sample.size(); ++tick)
	{
		if (played.sample[tick].lit && !BbtagScript::Speck(played.sample[tick].rect))
			return tick;
	}

	return 0;
}

size_t Seam(const BbtagScript::Played& played, size_t begin, size_t count)
{
	if (played.sample.empty())
		return count;

	const size_t last = std::min(begin + count, played.sample.size() - 1);

	for (size_t end = last; end > begin + count / 2; --end)
	{
		if (SameLook(played.sample[end], played.sample[begin]))
			return end - begin;
	}

	return count;
}

bool BbtagScript::Speck(const Rect& rect)
{
	return rect.w <= kLeastRect || rect.h <= kLeastRect;
}

bool BbtagScript::Sprites(const Played& played, Sprite& out)
{
	size_t count = played.sample.size();

	if (played.rolled || !played.cyclic)
		count = std::min(count, kPortRolledFrames);

	Collect(played, 0, count, out);

	if (!out.rect.empty() && out.rect.size() * out.frame.size() > kSpriteBudget)
		count = std::min(count, std::max(kPortRolledFrames, kSpriteBudget / out.rect.size()));

	if (played.rolled)
	{
		const size_t begin = FirstLit(played);
		Collect(played, begin, Seam(played, begin, count), out);
	}
	else if (out.frame.size() != count)
	{
		Collect(played, 0, count, out);
	}

	return !out.rect.empty() && out.loop > 1;
}

bool BbtagScript::Motions(const Played& played, Run& out)
{
	out = Run();

	const bool takes = std::any_of(played.named.begin(), played.named.end(),
		[](const std::string& one) { return EndsWith(one, ".mmot"); });

	const bool picks = std::any_of(played.sample.begin(), played.sample.end(),
		[](const Sample& one) { return one.take >= 0; });

	if (!takes || !picks)
		return false;

	for (const Sample& sample : played.sample)
	{
		Step step;
		step.take = sample.take >= 0 && sample.take < static_cast<int64_t>(played.named.size())
			? played.named[static_cast<size_t>(sample.take)] : std::string();
		step.at = static_cast<int>(sample.since);
		out.frame.push_back(step);
	}

	out.loop = static_cast<int>(out.frame.size());
	out.settled = !played.cyclic && played.from + 1 == static_cast<int>(played.sample.size());

	return out.loop > 0;
}

bool BbtagScript::Lamps(const Played& played, Lamp& out)
{
	std::vector<double> level;
	level.reserve(played.sample.size());

	for (const Sample& one : played.sample)
		level.push_back(one.ramp);

	return Timeline(level, played.cyclic ? 0 : played.from, out);
}

bool BbtagScript::Showing(const Played& played, const Rect& rect, Lamp& out)
{
	std::vector<double> level;
	level.reserve(played.sample.size());

	for (const Sample& one : played.sample)
	{
		const bool drawn = one.lit && SameRect(one.rect, rect);

		level.push_back(drawn ? one.ramp : 0.0);
	}

	return Timeline(level, played.cyclic ? 0 : played.from, out);
}

bool BbtagScript::Long(const Played& played)
{
	if (played.rolled)
		return false;

	return !played.cyclic || played.sample.size() > kLongRun;
}

bool BbtagScript::Tilt(const std::vector<uint8_t>& blob, float& degrees)
{
	if (blob.size() < kBlocks || memcmp(blob.data(), "EVT0", 4) != 0)
		return false;

	bool open = false;

	for (size_t at = Dword(blob, 0x10); at + kRecord <= blob.size(); at += kRecord)
	{
		const uint32_t code = Dword(blob, at);

		if (code == kNone)
			break;

		if (code == kSceneOpen || code == kSceneClose)
		{
			open = code == kSceneOpen;
			continue;
		}

		if (!open || code != kSceneTilt)
			continue;

		degrees = static_cast<float>(static_cast<int32_t>(Dword(blob, at + 4)));
		return true;
	}

	return false;
}

bool BbtagScript::Spawns(const std::vector<uint8_t>& blob, std::vector<Spawn>& out)
{
	out.clear();

	if (blob.size() < kBlocks || memcmp(blob.data(), "EVT0", 4) != 0)
		return false;

	std::vector<std::string> names;
	Block(blob, kParticleBlock, names);

	for (size_t at = Dword(blob, 0x10); at + kRecord <= blob.size(); at += kRecord)
	{
		const uint32_t code = Dword(blob, at);

		if (code == kNone)
			break;

		if (code != kSpawn)
			continue;

		const int64_t index = Signed(Dword(blob, at + 4));

		if (index < 0 || index >= static_cast<int64_t>(names.size()))
			continue;

		out.push_back({ names[static_cast<size_t>(index)], static_cast<int>(Signed(Dword(blob, at + 8))) });
	}

	return true;
}
