#include "Game/Stages/Bbtag/MmotWriter.h"

#include <cstring>

namespace {

constexpr uint32_t kVersion = 0x3eb;
constexpr int kSections = 10;
constexpr size_t kTable = 0x20;
constexpr size_t kTableStride = 0x10;
constexpr size_t kData = kTable + kSections * kTableStride;

constexpr int kHeader = 0;
constexpr int kBones = 1;
constexpr int kKeyList = 2;
constexpr int kKeys = 3;
constexpr int kLinked = 4;
constexpr int kLinkOrder = 5;
constexpr int kLinkSpare = 6;
constexpr int kSpare = 7;
constexpr int kStringInfo = 8;
constexpr int kStrings = 9;

constexpr size_t kBoneStride = 0xe0;
constexpr size_t kListStride = 0x10;
constexpr size_t kKeyStride = 0x20;
constexpr size_t kLinkStride = 0x20;
constexpr size_t kInfoStride = 0x10;

constexpr int kTranslation = 0;
constexpr int kRotation = 1;
constexpr int kTurn = 2;
constexpr int kScale = 3;

void Dword(std::vector<uint8_t>& out, uint32_t value)
{
	const uint8_t bytes[4] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24) };
	out.insert(out.end(), bytes, bytes + 4);
}

void Floats(std::vector<uint8_t>& out, const float* values, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		uint32_t bits = 0;
		memcpy(&bits, &values[i], 4);
		Dword(out, bits);
	}
}

void Pad(std::vector<uint8_t>& out, size_t start, size_t stride)
{
	while (out.size() < start + stride)
		out.push_back(0);
}

void Record(std::vector<uint8_t>& out, size_t stride, std::initializer_list<uint32_t> values)
{
	const size_t start = out.size();

	for (uint32_t value : values)
		Dword(out, value);

	Pad(out, start, stride);
}

MmotWriter::Key KeyOf(const float* value, int width, int frame)
{
	MmotWriter::Key out = {};
	memcpy(out.value, value, sizeof(float) * width);
	out.frame = frame;

	return out;
}

}

void MmotWriter::Hold(Bone& bone, const BbtagPose::Pose& pose, int frame)
{
	bone.track[kTranslation].push_back(KeyOf(pose.translation, 3, frame));
	bone.track[kRotation].push_back(KeyOf(pose.rotation, 3, frame));
	bone.track[kTurn].push_back(KeyOf(pose.turn, 4, frame));
	bone.track[kScale].push_back(KeyOf(pose.scale, 3, frame));
}

void MmotWriter::Build(const Take& take, std::vector<uint8_t>& out)
{
	std::vector<std::string> strings = { take.name, take.root };
	strings.insert(strings.end(), take.meshes.begin(), take.meshes.end());

	std::vector<uint8_t> body;
	uint32_t offsets[kSections] = {};
	uint32_t counts[kSections] = {};

	const auto mark = [&](int section, size_t count)
	{
		offsets[section] = static_cast<uint32_t>(kData + body.size());
		counts[section] = static_cast<uint32_t>(count);
	};

	mark(kBones, take.bones.size());
	uint32_t firstList = 0;

	for (const Bone& bone : take.bones)
	{
		const size_t start = body.size();
		Floats(body, bone.local, 16);
		Floats(body, bone.unbind, 16);
		Floats(body, bone.parentUnbind, 16);
		Dword(body, static_cast<uint32_t>(take.frames));

		for (int k = 0; k < kTracks; ++k)
			Dword(body, firstList + k);

		Pad(body, start, kBoneStride);
		firstList += kTracks;
	}

	mark(kKeyList, take.bones.size() * kTracks);
	uint32_t firstKey = 0;

	for (const Bone& bone : take.bones)
	{
		for (const std::vector<Key>& track : bone.track)
		{
			Record(body, kListStride, { firstKey, static_cast<uint32_t>(track.size()) });
			firstKey += static_cast<uint32_t>(track.size());
		}
	}

	mark(kKeys, firstKey);

	for (const Bone& bone : take.bones)
	{
		for (const std::vector<Key>& track : bone.track)
		{
			for (const Key& key : track)
			{
				const size_t start = body.size();
				Floats(body, key.value, 4);
				Dword(body, static_cast<uint32_t>(key.frame));
				Pad(body, start, kKeyStride);
			}
		}
	}

	const uint32_t firstMesh = 2;
	mark(kLinked, take.meshes.size());

	for (size_t i = 0; i < take.meshes.size(); ++i)
		Record(body, kLinkStride, { 0, firstMesh + static_cast<uint32_t>(i), static_cast<uint32_t>(i), 1 });

	mark(kLinkOrder, take.meshes.size());

	for (size_t i = 0; i < take.meshes.size(); ++i)
		Record(body, kLinkStride, { static_cast<uint32_t>(i) });

	mark(kLinkSpare, take.meshes.size());

	for (size_t i = 0; i < take.meshes.size(); ++i)
		Record(body, kLinkStride, {});

	mark(kSpare, 0);
	mark(kStringInfo, strings.size());
	uint32_t offset = 0;

	for (const std::string& text : strings)
	{
		Record(body, kInfoStride, { offset, static_cast<uint32_t>(text.size()) });
		offset += static_cast<uint32_t>(text.size() + 1);
	}

	mark(kStrings, offset);

	for (const std::string& text : strings)
	{
		body.insert(body.end(), text.begin(), text.end());
		body.push_back(0);
	}

	offsets[kHeader] = 0;
	counts[kHeader] = 1;

	out.clear();
	out.insert(out.end(), { 'M', 'M', 'O', 'T' });
	Dword(out, kVersion);
	Pad(out, 0, kTable);

	for (int i = 0; i < kSections; ++i)
		Record(out, kTableStride, { offsets[i], counts[i] });

	out.insert(out.end(), body.begin(), body.end());
}
