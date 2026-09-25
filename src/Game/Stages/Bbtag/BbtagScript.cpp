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

constexpr uint32_t kNone = 0xffffffffu;
constexpr uint32_t kYield = 0x02;
constexpr uint32_t kTime = 0x03;
constexpr uint32_t kClose = 0x04;
constexpr uint32_t kGroup = 0x05;
constexpr uint32_t kMotion = 0x06;
constexpr uint32_t kBegin = 0x09;
constexpr uint32_t kEnd = 0x0a;
constexpr uint32_t kRect = 0x0b;
constexpr uint32_t kJump = 0x0f;
constexpr uint32_t kRamp = 0x12;
constexpr uint32_t kRandom = 0x13;
constexpr uint32_t kRandomEnd = 0x14;
constexpr uint32_t kPause = 0x15;

constexpr int kFree = 0;
constexpr int kSkipped = 1;
constexpr int kTaken = 2;
constexpr int kDone = 3;

constexpr int kFrames = 12000;
constexpr int kRolledFrames = 3600;
constexpr int kRollTries = 16;
constexpr size_t kPortRolledFrames = 600;
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

bool Same(const BbtagScript::Sample& one, const BbtagScript::Sample& other)
{
	if (one.lit != other.lit || one.ramp != other.ramp || one.picked != other.picked)
		return false;

	if (one.picked && one.pick != other.pick)
		return false;

	if (!one.lit)
		return true;

	return one.rect.sheet == other.rect.sheet && one.rect.x == other.rect.x
		&& one.rect.y == other.rect.y && one.rect.w == other.rect.w && one.rect.h == other.rect.h;
}

typedef std::tuple<int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, bool,
	std::array<uint32_t, 5>, double, double, int64_t, double> State;

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

	void Play(std::vector<BbtagScript::Sample>& samples, bool& cyclic)
	{
		Interpret();
		Walk();

		std::map<State, int> seen;
		samples.clear();
		cyclic = false;

		for (int tick = 0; tick < kFrames; ++tick)
		{
			if (tick != 0)
				Tick();

			samples.push_back(Current());
			m_picked = false;

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
				Repeating(samples, known->second, tick, cyclic);
				return;
			}

			seen[state] = tick;
		}
	}

private:
	BbtagScript::Sample Current() const
	{
		BbtagScript::Sample sample = {};
		sample.picked = m_picked;
		sample.pick = m_pick;

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
			m_lit, m_lit ? m_rect : std::array<uint32_t, 5>(), m_ramp, m_target, m_left, m_step);
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
			m_picked = true;
			m_pick = Signed(fields[1]);
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

	static void Repeating(std::vector<BbtagScript::Sample>& samples, int first, int now, bool& cyclic)
	{
		const int period = now - first;
		int start = first + 1;

		while (start > 0 && Same(samples[start - 1], samples[start - 1 + period]))
			--start;

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

		while (static_cast<int>(samples.size()) < kFrames)
			samples.push_back(samples[samples.size() - period]);

		cyclic = false;
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
	bool m_picked = false;
	int64_t m_pick = 0;
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
		instance.Play(out.sample, out.cyclic);
		out.rolled = instance.Rolled();

		if (Shows(out.sample) || !out.rolled)
			break;
	}

	return true;
}

bool BbtagScript::Sprites(const Played& played, Sprite& out)
{
	out = Sprite();
	out.sheets = played.sheets;

	size_t count = played.sample.size();

	if (played.rolled || !played.cyclic)
		count = std::min(count, kPortRolledFrames);

	for (size_t tick = 0; tick < count; ++tick)
	{
		const Sample& sample = played.sample[tick];

		if (!sample.lit)
		{
			out.frame.push_back(-1);
			continue;
		}

		int index = -1;

		for (size_t i = 0; i < out.rect.size() && index < 0; ++i)
		{
			const Rect& rect = out.rect[i];

			if (rect.sheet == sample.rect.sheet && rect.x == sample.rect.x && rect.y == sample.rect.y
				&& rect.w == sample.rect.w && rect.h == sample.rect.h)
			{
				index = static_cast<int>(i);
			}
		}

		if (index < 0)
		{
			index = static_cast<int>(out.rect.size());
			out.rect.push_back(sample.rect);
		}

		out.frame.push_back(index);
	}

	out.loop = static_cast<int>(out.frame.size());

	return !out.rect.empty() && out.loop > 1;
}

bool BbtagScript::Motions(const Played& played, Run& out)
{
	out = Run();

	const bool takes = std::any_of(played.named.begin(), played.named.end(),
		[](const std::string& one) { return EndsWith(one, ".mmot"); });

	const bool picks = std::any_of(played.sample.begin(), played.sample.end(),
		[](const Sample& one) { return one.picked; });

	if (!takes || !picks)
		return false;

	std::string take;
	int began = 0;

	for (size_t tick = 0; tick < played.sample.size(); ++tick)
	{
		const Sample& sample = played.sample[tick];

		if (sample.picked)
		{
			began = static_cast<int>(tick);
			take = sample.pick >= 0 && sample.pick < static_cast<int64_t>(played.named.size())
				? played.named[static_cast<size_t>(sample.pick)] : std::string();
		}

		Step step;
		step.take = take;
		step.at = static_cast<int>(tick) - began;
		out.frame.push_back(step);
	}

	out.loop = static_cast<int>(out.frame.size());

	return out.loop > 1;
}

bool BbtagScript::Lamps(const Played& played, Lamp& out)
{
	out = Lamp();

	const std::vector<Sample>& sample = played.sample;

	if (!played.cyclic || sample.size() < 2)
		return false;

	bool fades = false;

	for (const Sample& one : sample)
	{
		if (one.lit)
			return false;

		fades = fades || one.ramp != sample[0].ramp;
	}

	if (!fades)
		return false;

	std::vector<int> frames(1, 0);
	std::vector<double> kept(1, sample[0].ramp);

	for (size_t tick = 1; tick + 1 < sample.size(); ++tick)
	{
		const double bend = (sample[tick].ramp - sample[tick - 1].ramp)
			- (sample[tick + 1].ramp - sample[tick].ramp);

		if (std::fabs(bend) > kLampBend)
		{
			frames.push_back(static_cast<int>(tick));
			kept.push_back(sample[tick].ramp);
		}
	}

	frames.push_back(static_cast<int>(sample.size()) - 1);
	kept.push_back(sample.back().ramp);
	frames.push_back(static_cast<int>(sample.size()));
	kept.push_back(sample[0].ramp);

	for (size_t i = 0; i + 1 < frames.size(); ++i)
	{
		Ramp ramp = {};
		ramp.at = frames[i];
		ramp.target = static_cast<int>(std::floor(kept[i + 1] + 0.5));
		ramp.frames = frames[i + 1] - frames[i];
		out.ramp.push_back(ramp);
	}

	if (out.ramp.size() > kLampRamps)
	{
		out = Lamp();
		return false;
	}

	out.loop = static_cast<int>(sample.size());

	return true;
}
