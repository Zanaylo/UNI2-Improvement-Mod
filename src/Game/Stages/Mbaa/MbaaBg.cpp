#include "Game/Stages/Mbaa/MbaaBg.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr const char* kMagic = "bgmake";
constexpr size_t kMagicBytes = 6;
constexpr size_t kCgOffsetAt = 0x1c;
constexpr size_t kCgBytesAt = 0x20;
constexpr size_t kObjectTableAt = 0x54;
constexpr int kObjectSlots = 256;
constexpr size_t kConditionsAt = 0x0c;
constexpr size_t kEventsAt = 0x10;
constexpr size_t kPlacedAt = 0x14;
constexpr size_t kLayerHeader = 0x3c;
constexpr size_t kFrameBytes = 0x84;
constexpr size_t kRecordBytes = 0x34;
constexpr int kMostFrames = 256;
constexpr int kReferences = 8;
constexpr size_t kMotionAt = 0x2c;
constexpr size_t kVelocityAt = 0x34;
constexpr size_t kAccelerationAt = 0x3c;
constexpr size_t kConditionListAt = 0x64;
constexpr size_t kEventListAt = 0x74;
constexpr int kOpaque = 255;

uint32_t Dword(const std::vector<uint8_t>& data, size_t at)
{
	uint32_t value = 0;

	if (at + 4 <= data.size())
		memcpy(&value, &data[at], 4);

	return value;
}

int Short(const std::vector<uint8_t>& data, size_t at)
{
	int16_t value = 0;

	if (at + 2 <= data.size())
		memcpy(&value, &data[at], 2);

	return value;
}

int Word(const std::vector<uint8_t>& data, size_t at)
{
	uint16_t value = 0;

	if (at + 2 <= data.size())
		memcpy(&value, &data[at], 2);

	return value;
}

std::vector<int> References(const std::vector<uint8_t>& dat, size_t at)
{
	std::vector<int> out;

	for (int i = 0; i < kReferences; ++i)
	{
		const int reference = Short(dat, at + static_cast<size_t>(i) * 2);

		if (reference >= 0)
			out.push_back(reference);
	}

	return out;
}

bool ReadFrame(const std::vector<uint8_t>& dat, size_t at, MbaaBg::Frame& out)
{
	if (at + kFrameBytes > dat.size())
		return false;

	out.image = Word(dat, at);
	out.x = Short(dat, at + 2);
	out.y = Short(dat, at + 4);
	out.duration = Word(dat, at + 6);
	out.blend = dat[at + 9];
	out.alpha = dat[at + 10];
	out.op = dat[at + 11];
	out.jump = dat[at + 12];
	out.tween = dat[at + 0x14];
	out.exit = dat[at + 0x15];
	out.loops = dat[at + 0x16];
	out.stopX = dat[at + kMotionAt] != 0;
	out.stopY = dat[at + kMotionAt + 1] != 0;
	out.pushX = dat[at + kMotionAt + 2] != 0;
	out.pushY = dat[at + kMotionAt + 3] != 0;

	for (int axis = 0; axis < 2; ++axis)
	{
		out.velocity[axis] = Short(dat, at + kVelocityAt + static_cast<size_t>(axis) * 2);
		out.acceleration[axis] = Short(dat, at + kAccelerationAt + static_cast<size_t>(axis) * 2);
	}

	out.conditions = References(dat, at + kConditionListAt);
	out.events = References(dat, at + kEventListAt);

	return true;
}

int MostReferenced(const std::vector<MbaaBg::Frame>& frames, std::vector<int> MbaaBg::Frame::*list)
{
	int most = -1;

	for (const MbaaBg::Frame& frame : frames)
	{
		for (int reference : frame.*list)
			most = (std::max)(most, reference);
	}

	return most;
}

std::vector<MbaaBg::Record> ReadRecords(const std::vector<uint8_t>& dat, size_t at, int count)
{
	std::vector<MbaaBg::Record> out;

	for (int i = 0; i < count; ++i)
	{
		const size_t record = at + static_cast<size_t>(i) * kRecordBytes;

		if (record + kRecordBytes > dat.size())
			break;

		MbaaBg::Record entry = {};
		entry.kind = Short(dat, record);
		entry.target = Short(dat, record + 2);

		for (int v = 0; v < MbaaBg::kRecordValues; ++v)
			entry.values[v] = static_cast<int>(Dword(dat, record + 4 + static_cast<size_t>(v) * 4));

		out.push_back(entry);
	}

	return out;
}

bool ReadLayer(const std::vector<uint8_t>& dat, int object, size_t at, MbaaBg::Layer& out)
{
	if (at + kLayerHeader > dat.size())
		return false;

	const int count = static_cast<int>(Dword(dat, at));

	out.object = object;
	out.parallax = static_cast<int>(Dword(dat, at + 4));
	out.priority = static_cast<int>(Dword(dat, at + 8));
	out.placed = dat[at + kPlacedAt] == 0;

	for (int i = 0; i < count && i < kMostFrames; ++i)
	{
		MbaaBg::Frame frame = {};

		if (!ReadFrame(dat, at + kLayerHeader + static_cast<size_t>(i) * kFrameBytes, frame))
			break;

		out.frames.push_back(frame);
	}

	out.events = ReadRecords(dat, at + Dword(dat, at + kEventsAt),
		MostReferenced(out.frames, &MbaaBg::Frame::events) + 1);
	out.conditions = ReadRecords(dat, at + Dword(dat, at + kConditionsAt),
		MostReferenced(out.frames, &MbaaBg::Frame::conditions) + 1);

	return !out.frames.empty();
}

}

bool MbaaBg::Read(const std::vector<uint8_t>& dat, File& out)
{
	out = File();

	if (dat.size() < kObjectTableAt + kObjectSlots * 4 || memcmp(dat.data(), kMagic, kMagicBytes) != 0)
		return false;

	out.cgAt = Dword(dat, kCgOffsetAt);
	out.cgBytes = Dword(dat, kCgBytesAt);

	if (out.cgAt == 0 || out.cgAt + out.cgBytes > dat.size())
		return false;

	for (int object = 0; object < kObjectSlots; ++object)
	{
		const uint32_t at = Dword(dat, kObjectTableAt + static_cast<size_t>(object) * 4);
		Layer layer = {};

		if (at == 0xffffffffu || !ReadLayer(dat, object, at, layer))
			continue;

		out.layers.push_back(layer);
	}

	return !out.layers.empty();
}

const MbaaBg::Layer* MbaaBg::LayerOf(const File& file, int object)
{
	for (const Layer& layer : file.layers)
	{
		if (layer.object == object)
			return &layer;
	}

	return nullptr;
}

bool MbaaBg::IsSprite(const Frame& frame)
{
	return frame.image >= kSpriteBase;
}

bool MbaaBg::IsVisible(const Frame& frame)
{
	return IsSprite(frame) && AlphaOf(frame) > 0;
}

bool MbaaBg::IsAdditive(const Frame& frame)
{
	return frame.blend == Blend_Add || frame.blend == Blend_AddStrong;
}

int MbaaBg::AlphaOf(const Frame& frame)
{
	return frame.blend == Blend_Solid ? kOpaque : frame.alpha;
}

float MbaaBg::Opacity(const Frame& frame)
{
	return AlphaOf(frame) / static_cast<float>(kOpaque);
}
