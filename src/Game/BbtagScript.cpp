#include "Game/BbtagScript.h"

#include <cstring>

#include <string.h>

namespace {

constexpr size_t kHeader = 0x20;
constexpr size_t kRecord = 0x20;
constexpr size_t kBlocks = 0x30;
constexpr int kUnused = -1;

constexpr uint32_t kNotSupplied = 0xffffffffu;

constexpr uint32_t kTimelineEnd = 0x02;
constexpr uint32_t kTime = 0x03;
constexpr uint32_t kGroup = 0x05;
constexpr uint32_t kMotion = 0x06;
constexpr uint32_t kLoop = 0x0f;
constexpr uint32_t kBegin = 0x09;
constexpr uint32_t kEnd = 0x0a;
constexpr uint32_t kSprite = 0x0b;
constexpr uint32_t kRamp = 0x12;
constexpr int kFull = 1000;

constexpr int kMostFrames = 12000;

struct Frame
{
	int held;
	BbtagScript::Rect rect;
};

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

bool Names(const std::vector<uint8_t>& blob, std::vector<std::string>& out)
{
	out.clear();

	const size_t stride = Dword(blob, 0x14);
	const size_t skipped = Word(blob, 0x20);
	const size_t count = Word(blob, 0x22);

	if (stride == 0 || count == 0)
		return false;

	const size_t first = kBlocks + skipped * stride;

	if (first + count * stride > blob.size())
		return false;

	for (size_t i = 0; i < count; ++i)
	{
		const size_t at = first + i * stride;
		std::string one;

		for (size_t k = 0; k < stride && blob[at + k] != 0; ++k)
		{
			const uint8_t letter = blob[at + k];

			if (letter < 0x20 || letter >= 0x7f)
				return false;

			one.push_back(static_cast<char>(letter));
		}

		out.push_back(one);
	}

	return true;
}

int Signed(uint32_t value)
{
	return value == kNotSupplied ? kUnused : static_cast<int>(value);
}

}

bool BbtagScript::Read(const std::vector<uint8_t>& blob, Sprite& out)
{
	out.loop = 0;
	out.frame.clear();
	out.rect.clear();

	if (blob.size() < kHeader + 8 || memcmp(blob.data(), "EVT0", 4) != 0)
		return false;

	const size_t commands = Dword(blob, 0x10);

	if (commands + kRecord > blob.size())
		return false;

	std::vector<std::pair<int, int> > entries;
	std::map<int, std::vector<Frame> > groups;

	int loop = 0;
	int time = 0;
	int current = kUnused;
	bool timeline = true;

	for (size_t at = commands; at + kRecord <= blob.size(); at += kRecord)
	{
		const uint32_t code = Dword(blob, at);

		if (code == kNotSupplied)
			break;

		if (timeline)
		{
			if (code == kTimelineEnd)
				timeline = false;
			else if (code == kTime)
				time = Signed(Dword(blob, at + 4));
			else if (code == kGroup)
				entries.push_back(std::make_pair(time, Signed(Dword(blob, at + 4))));
			else if (code == kLoop)
				loop = time;

			continue;
		}

		if (code == kBegin)
		{
			current = Signed(Dword(blob, at + 4));
			groups[current];
			continue;
		}

		if (code == kEnd)
		{
			current = kUnused;
			continue;
		}

		if (code != kSprite || current == kUnused)
			continue;

		Frame frame = {};
		frame.held = Signed(Dword(blob, at + 4));
		frame.rect.x = Signed(Dword(blob, at + 12));
		frame.rect.y = Signed(Dword(blob, at + 16));
		frame.rect.w = Signed(Dword(blob, at + 20));
		frame.rect.h = Signed(Dword(blob, at + 24));

		groups[current].push_back(frame);
	}

	if (entries.empty() || loop < 2 || loop > kMostFrames)
		return false;

	out.loop = loop;
	out.frame.assign(loop, kUnused);

	for (int at = 0; at < loop; ++at)
	{
		int chosen = kUnused;
		int began = 0;

		for (const std::pair<int, int>& entry : entries)
		{
			if (entry.first > at)
				continue;

			chosen = entry.second;
			began = entry.first;
		}

		const std::map<int, std::vector<Frame> >::const_iterator group = groups.find(chosen);

		if (group == groups.end() || group->second.empty())
			continue;

		int span = 0;

		for (const Frame& frame : group->second)
			span += frame.held > 0 ? frame.held : 0;

		if (span <= 0)
			continue;

		int step = (at - began) % span;

		for (const Frame& frame : group->second)
		{
			if (step < frame.held)
			{
				int index = kUnused;

				for (size_t i = 0; i < out.rect.size(); ++i)
				{
					if (out.rect[i].x == frame.rect.x && out.rect[i].y == frame.rect.y
						&& out.rect[i].w == frame.rect.w && out.rect[i].h == frame.rect.h)
					{
						index = static_cast<int>(i);
						break;
					}
				}

				if (index == kUnused)
				{
					index = static_cast<int>(out.rect.size());
					out.rect.push_back(frame.rect);
				}

				out.frame[at] = index;
				break;
			}

			step -= frame.held;
		}
	}

	return true;
}

bool BbtagScript::ReadRun(const std::vector<uint8_t>& blob, Run& out)
{
	out.loop = 0;
	out.frame.clear();

	if (blob.size() < kHeader + 8 || memcmp(blob.data(), "EVT0", 4) != 0)
		return false;

	const size_t commands = Dword(blob, 0x10);

	if (commands + kRecord > blob.size())
		return false;

	std::vector<std::string> named;

	if (!Names(blob, named))
		return false;

	bool takes = false;

	for (const std::string& one : named)
	{
		if (one.size() > 5 && _stricmp(one.c_str() + one.size() - 5, ".mmot") == 0)
			takes = true;
	}

	if (!takes)
		return false;

	std::vector<std::pair<int, int> > entries;
	int loop = 0;
	int time = 0;

	for (size_t at = commands; at + kRecord <= blob.size(); at += kRecord)
	{
		const uint32_t code = Dword(blob, at);

		if (code == kNotSupplied || code == kTimelineEnd)
			break;

		if (code == kTime)
			time = Signed(Dword(blob, at + 4));
		else if (code == kMotion)
			entries.push_back(std::make_pair(time, Signed(Dword(blob, at + 4))));
		else if (code == kLoop)
			loop = time;
	}

	if (entries.empty() || loop < 2 || loop > kMostFrames)
		return false;

	out.loop = loop;

	for (int at = 0; at < loop; ++at)
	{
		int chosen = kUnused;
		int began = 0;

		for (const std::pair<int, int>& entry : entries)
		{
			if (entry.first > at)
				continue;

			chosen = entry.second;
			began = entry.first;
		}

		Step step;
		step.at = at - began;

		if (chosen >= 0 && chosen < static_cast<int>(named.size()))
			step.take = named[chosen];

		out.frame.push_back(step);
	}

	return true;
}

bool BbtagScript::ReadLamp(const std::vector<uint8_t>& blob, Lamp& out)
{
	out.loop = 0;
	out.ramp.clear();

	if (blob.size() < kHeader + 8 || memcmp(blob.data(), "EVT0", 4) != 0)
		return false;

	const size_t commands = Dword(blob, 0x10);

	if (commands + kRecord > blob.size())
		return false;

	int time = 0;

	for (size_t at = commands; at + kRecord <= blob.size(); at += kRecord)
	{
		const uint32_t code = Dword(blob, at);

		if (code == kNotSupplied || code == kTimelineEnd)
			break;

		if (code == kTime)
		{
			time = Signed(Dword(blob, at + 4));
			continue;
		}

		if (code == kLoop)
		{
			out.loop = time;
			break;
		}

		if (code != kRamp)
			continue;

		Ramp ramp = {};
		ramp.at = time;
		ramp.target = Signed(Dword(blob, at + 4));
		ramp.frames = Signed(Dword(blob, at + 8));

		if (ramp.frames < 1)
			ramp.frames = 1;

		out.ramp.push_back(ramp);
	}

	return out.loop > 1 && out.loop <= kMostFrames && !out.ramp.empty();
}

float BbtagScript::LampAt(const Lamp& lamp, int frame)
{
	if (lamp.loop < 1 || lamp.ramp.empty())
		return 1.0f;

	const int at = ((frame % lamp.loop) + lamp.loop) % lamp.loop;

	float held = static_cast<float>(lamp.ramp.back().target) / kFull;
	float value = held;

	for (const Ramp& ramp : lamp.ramp)
	{
		if (ramp.at > at)
			break;

		const float target = static_cast<float>(ramp.target) / kFull;
		const int step = at - ramp.at;

		value = step >= ramp.frames ? target
			: held + (target - held) * (static_cast<float>(step) / ramp.frames);
		held = target;
	}

	return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

const BbtagScript::Run* BbtagScript::RunFor(const std::map<std::string, Run>& held,
	const std::string& mesh)
{
	for (const std::pair<const std::string, Run>& one : held)
	{
		if (mesh.compare(0, one.first.size(), one.first) == 0)
			return &one.second;
	}

	return nullptr;
}

const BbtagScript::Sprite* BbtagScript::For(const std::map<std::string, Sprite>& held,
	const std::string& mesh)
{
	for (const std::pair<const std::string, Sprite>& one : held)
	{
		if (mesh.compare(0, one.first.size(), one.first) == 0)
			return &one.second;
	}

	return nullptr;
}
