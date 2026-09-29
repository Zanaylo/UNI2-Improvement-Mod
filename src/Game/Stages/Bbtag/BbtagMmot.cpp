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
constexpr size_t kBoneStride = 0xe0;
constexpr size_t kUnbindAt = 0x40;

constexpr size_t kTranslation = 0x04;
constexpr size_t kRotation = 0x14;
constexpr size_t kTurn = 0x24;
constexpr size_t kScale = 0x34;

constexpr float kSlerpFlat = 0.001f;

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

int Width(BbtagMmot::Motion::Kind kind)
{
	return kind == BbtagMmot::Motion::Kind::Turn ? 4 : 3;
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

void Euler(const float rotate[3], float out[16])
{
	BbtagMua::Identity(out);

	const int order[3] = { 1, 0, 2 };

	for (int step = 0; step < 3; ++step)
	{
		float turn[16] = {};
		Rotation(order[step], rotate[order[step]], turn);

		float composed[16] = {};
		BbtagMua::Multiply(out, turn, composed);
		memcpy(out, composed, sizeof(float) * 16);
	}
}

void Quaternion(const float q[4], float out[16])
{
	BbtagMua::Identity(out);

	const float x = q[0];
	const float y = q[1];
	const float z = q[2];
	const float w = q[3];

	out[0] = 1.0f - 2.0f * (y * y + z * z);
	out[1] = 2.0f * (x * y + z * w);
	out[2] = 2.0f * (x * z - y * w);
	out[4] = 2.0f * (x * y - z * w);
	out[5] = 1.0f - 2.0f * (x * x + z * z);
	out[6] = 2.0f * (y * z + x * w);
	out[8] = 2.0f * (x * z + y * w);
	out[9] = 2.0f * (y * z - x * w);
	out[10] = 1.0f - 2.0f * (x * x + y * y);
}

void Slerp(const std::vector<float>& from, const std::vector<float>& to, float t, float out[4])
{
	float low = 1.0f - t;
	float high = t;
	float dot = 0.0f;

	for (int k = 0; k < 4; ++k)
		dot += from[k] * to[k];

	if (dot < 0.0f)
	{
		high = -high;
		dot = -dot;
	}

	if (1.0f - dot > kSlerpFlat)
	{
		const float theta = acosf(dot);
		low = sinf(theta * low) / sinf(theta);
		high = sinf(theta * high) / sinf(theta);
	}

	for (int k = 0; k < 4; ++k)
		out[k] = low * from[k] + high * to[k];
}

void Copy(const std::vector<float>& value, int width, float* out)
{
	for (int i = 0; i < width; ++i)
		out[i] = i < static_cast<int>(value.size()) ? value[i] : 0.0f;
}

}

BbtagMmot::Motion::Track& BbtagMmot::Motion::Of(BoneTracks& tracks, Kind kind)
{
	if (kind == Kind::Translation)
		return tracks.translation;

	if (kind == Kind::Rotation)
		return tracks.rotation;

	if (kind == Kind::Turn)
		return tracks.turn;

	return tracks.scale;
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
	m_unbind.assign(m_bones, std::vector<float>(16, 0.0f));

	for (int bone = 0; bone < m_bones; ++bone)
	{
		for (int k = 0; k < 16; ++k)
			m_unbind[bone][k] = Single(blob, offset[kBones] + bone * kBoneStride + kUnbindAt + k * 4);
	}

	const size_t list = offset[kKeyList];
	size_t at = offset[kKeys];

	m_track.assign(m_bones, BoneTracks());
	m_frames = 0;

	const size_t lists[4] = { kTranslation, kRotation, kTurn, kScale };
	const Kind kinds[4] = { Kind::Translation, Kind::Rotation, Kind::Turn, Kind::Scale };

	for (int bone = 0; bone < m_bones; ++bone)
	{
		for (int which = 0; which < 4; ++which)
		{
			const uint32_t keys = Dword(blob, list + bone * kListStride + lists[which]);
			Track& target = Of(m_track[bone], kinds[which]);
			const int width = Width(kinds[which]);

			for (uint32_t i = 0; i < keys; ++i)
			{
				if (at + kKeyStride > blob.size())
					return false;

				const int frame = static_cast<int>(Dword(blob, at + 0x10));
				std::vector<float> value;

				for (int k = 0; k < width; ++k)
					value.push_back(Single(blob, at + k * 4));

				target.key[frame] = value;
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

	const std::map<int, std::vector<float> >::const_iterator above = track.key.lower_bound(frame);

	if (above != track.key.end() && above->first == frame)
	{
		Copy(above->second, 3, out);
		return;
	}

	if (above == track.key.begin())
	{
		Copy(above->second, 3, out);
		return;
	}

	std::map<int, std::vector<float> >::const_iterator below = above;
	--below;

	if (above == track.key.end())
	{
		Copy(below->second, 3, out);
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

void BbtagMmot::Motion::Turned(const Track& track, int frame, float out[4]) const
{
	const std::map<int, std::vector<float> >::const_iterator above = track.key.lower_bound(frame);

	if (above != track.key.end() && above->first == frame)
	{
		Copy(above->second, 4, out);
		return;
	}

	if (above == track.key.begin())
	{
		Copy(above->second, 4, out);
		return;
	}

	std::map<int, std::vector<float> >::const_iterator below = above;
	--below;

	if (above == track.key.end() || below->second.size() < 4 || above->second.size() < 4)
	{
		Copy(below->second, 4, out);
		return;
	}

	const float span = static_cast<float>(above->first - below->first);
	const float t = span == 0.0f ? 0.0f : (frame - below->first) / span;

	Slerp(below->second, above->second, t, out);
}

void BbtagMmot::Motion::Reset(int bones, int frames)
{
	m_target.clear();
	m_bones = bones < 0 ? 0 : bones;
	m_frames = frames < 0 ? 0 : frames;
	m_track.assign(static_cast<size_t>(m_bones), BoneTracks());
	m_unbind.clear();
}

bool BbtagMmot::Motion::Unbind(int bone, float out[16]) const
{
	if (bone < 0 || bone >= static_cast<int>(m_unbind.size()) || m_unbind[bone][15] == 0.0f)
		return false;

	memcpy(out, m_unbind[bone].data(), sizeof(float) * 16);

	return true;
}

void BbtagMmot::Motion::Add(int bone, Kind kind, const float value[4], int frame)
{
	if (bone < 0 || bone >= static_cast<int>(m_track.size()) || frame < 0)
		return;

	Of(m_track[bone], kind).key[frame] = std::vector<float>(value, value + Width(kind));
}

void BbtagMmot::Motion::Pose(int bone, int frame, const float restTranslate[3],
	const float restRotate[3], const float restScale[3], float out[16]) const
{
	static const BoneTracks kUnkeyed;
	const BoneTracks& tracks = bone >= 0 && bone < static_cast<int>(m_track.size())
		? m_track[bone] : kUnkeyed;

	float translate[3] = {};
	float scale[3] = {};
	Value(tracks.translation, frame, restTranslate, translate);
	Value(tracks.scale, frame, restScale, scale);

	float turn[16] = {};

	if (tracks.turn.key.empty())
	{
		float rotate[3] = {};
		Value(tracks.rotation, frame, restRotate, rotate);
		Euler(rotate, turn);
	}
	else
	{
		float quaternion[4] = {};
		Turned(tracks.turn, frame, quaternion);
		Quaternion(quaternion, turn);
	}

	BbtagMua::Identity(out);

	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
			out[r * 4 + c] = scale[r] * turn[r * 4 + c];
	}

	out[12] = translate[0];
	out[13] = translate[1];
	out[14] = translate[2];
}
