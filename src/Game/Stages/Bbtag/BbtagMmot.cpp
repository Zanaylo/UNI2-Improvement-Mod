#include "Game/Stages/Bbtag/BbtagMmot.h"

#include "Game/Stages/Bbtag/BbtagMua.h"

#include <cmath>
#include <cstring>

namespace {

constexpr int kSections = 11;
constexpr int kBones = 1;
constexpr int kKeyList = 2;
constexpr int kKeys = 3;
constexpr int kStringInfo = 8;
constexpr int kStrings = 9;

constexpr size_t kHeader = 0x20;
constexpr size_t kSectionStride = 0x10;
constexpr size_t kKeyStride = 0x20;
constexpr size_t kListStride = 0x40;

constexpr size_t kTranslation = 0x04;
constexpr size_t kRotation = 0x14;
constexpr size_t kShear = 0x24;
constexpr size_t kScale = 0x34;

uint32_t Dword(const std::vector<uint8_t>& blob, size_t at)
{
	if (at + 4 > blob.size())
		return 0;

	uint32_t value = 0;
	memcpy(&value, blob.data() + at, 4);

	return value;
}

float Single(const std::vector<uint8_t>& blob, size_t at)
{
	if (at + 4 > blob.size())
		return 0.0f;

	float value = 0.0f;
	memcpy(&value, blob.data() + at, 4);

	return value;
}

void Rotation(int axis, float angle, float out[16])
{
	BbtagMua::Identity(out);

	const float cosine = cosf(angle);
	const float sine = sinf(angle);

	if (axis == 0)
	{
		out[5] = cosine;
		out[6] = sine;
		out[9] = -sine;
		out[10] = cosine;
		return;
	}

	if (axis == 1)
	{
		out[0] = cosine;
		out[2] = -sine;
		out[8] = sine;
		out[10] = cosine;
		return;
	}

	out[0] = cosine;
	out[1] = sine;
	out[4] = -sine;
	out[5] = cosine;
}

}

bool BbtagMmot::Motion::Read(const std::vector<uint8_t>& blob)
{
	if (blob.size() < kHeader + kSections * kSectionStride || memcmp(blob.data(), "MMOT", 4) != 0)
		return false;

	uint32_t offset[kSections] = {};
	uint32_t count[kSections] = {};

	for (int i = 0; i < kSections; ++i)
	{
		offset[i] = Dword(blob, kHeader + i * kSectionStride);
		count[i] = Dword(blob, kHeader + i * kSectionStride + 4);
	}

	m_bones = static_cast<int>(count[kBones]);

	if (m_bones <= 0 || m_bones > 4096)
		return false;

	const size_t base = offset[kStrings];
	std::vector<std::string> strings;

	for (uint32_t i = 0; i < count[kStringInfo]; ++i)
	{
		const size_t at = offset[kStringInfo] + i * 0x10;
		const size_t where = base + Dword(blob, at);
		const size_t length = Dword(blob, at + 4);

		std::string text;

		for (size_t k = 0; k < length && where + k < blob.size(); ++k)
		{
			const char letter = static_cast<char>(blob[where + k]);

			if (letter == 0)
				break;

			text.push_back(letter);
		}

		strings.push_back(text);
	}

	m_target = strings.size() > 1 ? strings[1] : std::string();

	const size_t list = offset[kKeyList];
	size_t at = offset[kKeys];

	m_track.assign(m_bones, BoneTracks());
	m_frames = 0;

	const size_t kinds[4] = { kTranslation, kRotation, kShear, kScale };
	const int widths[4] = { 3, 3, 4, 3 };

	for (int bone = 0; bone < m_bones; ++bone)
	{
		for (int which = 0; which < 4; ++which)
		{
			const uint32_t keys = Dword(blob, list + bone * kListStride + kinds[which]);

			Track* target = nullptr;

			if (kinds[which] == kTranslation)
				target = &m_track[bone].translation;
			else if (kinds[which] == kRotation)
				target = &m_track[bone].rotation;
			else if (kinds[which] == kScale)
				target = &m_track[bone].scale;

			for (uint32_t i = 0; i < keys; ++i)
			{
				if (at + kKeyStride > blob.size())
					return false;

				const int frame = static_cast<int>(Dword(blob, at + 0x10));

				if (target != nullptr)
				{
					std::vector<float> value;

					for (int k = 0; k < widths[which]; ++k)
						value.push_back(Single(blob, at + k * 4));

					target->key[frame] = value;
				}

				m_frames = frame + 1 > m_frames ? frame + 1 : m_frames;
				at += kKeyStride;
			}
		}
	}

	return true;
}

void BbtagMmot::Motion::Value(const Track& track, int frame, const float fallback[3],
	float out[3]) const
{
	if (track.key.empty())
	{
		for (int i = 0; i < 3; ++i)
			out[i] = fallback[i];

		return;
	}

	const std::map<int, std::vector<float> >::const_iterator exact = track.key.find(frame);

	if (exact != track.key.end())
	{
		for (int i = 0; i < 3; ++i)
			out[i] = i < static_cast<int>(exact->second.size()) ? exact->second[i] : 0.0f;

		return;
	}

	std::map<int, std::vector<float> >::const_iterator above = track.key.upper_bound(frame);

	if (above == track.key.begin())
	{
		for (int i = 0; i < 3; ++i)
			out[i] = i < static_cast<int>(above->second.size()) ? above->second[i] : 0.0f;

		return;
	}

	std::map<int, std::vector<float> >::const_iterator below = above;
	--below;

	if (above == track.key.end())
	{
		for (int i = 0; i < 3; ++i)
			out[i] = i < static_cast<int>(below->second.size()) ? below->second[i] : 0.0f;

		return;
	}

	const float span = static_cast<float>(above->first - below->first);
	const float t = span == 0.0f ? 0.0f : (frame - below->first) / span;

	for (int i = 0; i < 3; ++i)
	{
		const float low = i < static_cast<int>(below->second.size()) ? below->second[i] : 0.0f;
		const float high = i < static_cast<int>(above->second.size()) ? above->second[i] : 0.0f;

		out[i] = low + (high - low) * t;
	}
}

void BbtagMmot::Motion::Reset(int bones, int frames)
{
	m_target.clear();
	m_bones = bones < 0 ? 0 : bones;
	m_frames = frames < 0 ? 0 : frames;
	m_track.assign(static_cast<size_t>(m_bones), BoneTracks());
}

void BbtagMmot::Motion::Add(int bone, Kind kind, const float value[3], int frame)
{
	if (bone < 0 || bone >= static_cast<int>(m_track.size()) || frame < 0)
		return;

	BoneTracks& tracks = m_track[bone];
	Track& track = kind == Kind::Translation ? tracks.translation
		: kind == Kind::Rotation ? tracks.rotation : tracks.scale;

	track.key[frame] = std::vector<float>(value, value + 3);
}

void BbtagMmot::Motion::Sample(int bone, int frame, const float restTranslate[3],
	const float restRotate[3], const float restScale[3], float translate[3], float rotate[3],
	float scale[3]) const
{
	if (bone < 0 || bone >= static_cast<int>(m_track.size()))
		return;

	Value(m_track[bone].translation, frame, restTranslate, translate);
	Value(m_track[bone].rotation, frame, restRotate, rotate);
	Value(m_track[bone].scale, frame, restScale, scale);
}

void BbtagMmot::Compose(const float translate[3], const float rotate[3], const float scale[3],
	float out[16])
{
	BbtagMua::Identity(out);

	out[0] = scale[0];
	out[5] = scale[1];
	out[10] = scale[2];

	const int order[3] = { 1, 0, 2 };

	for (int step = 0; step < 3; ++step)
	{
		float turn[16] = {};
		Rotation(order[step], rotate[order[step]], turn);

		float composed[16] = {};
		BbtagMua::Multiply(out, turn, composed);
		memcpy(out, composed, sizeof(float) * 16);
	}

	out[12] = translate[0];
	out[13] = translate[1];
	out[14] = translate[2];
}
