#include "Game/BbtagMua.h"

#include <cmath>
#include <cstring>

namespace {

constexpr int kSections = 17;
constexpr int kSkeleton = 0;
constexpr int kScript = 11;
constexpr int kBone = 1;
constexpr int kMesh = 2;
constexpr int kPart = 3;
constexpr int kMaterial = 4;
constexpr int kAssign = 5;
constexpr int kTexture = 6;
constexpr int kUvAnim = 7;
constexpr int kAnimList = 8;
constexpr int kAnimKey = 9;
constexpr int kVertex = 13;
constexpr int kIndex = 14;
constexpr int kStringInfo = 15;
constexpr int kString = 16;

constexpr size_t kVertexBytes = 0x50;
constexpr size_t kHeader = 0x20;

}

uint32_t BbtagMua::Model::Dword(size_t at) const
{
	if (at + 4 > m_blob.size())
		return 0;

	uint32_t value = 0;
	memcpy(&value, m_blob.data() + at, 4);

	return value;
}

float BbtagMua::Model::Float(size_t at) const
{
	if (at + 4 > m_blob.size())
		return 0.0f;

	float value = 0.0f;
	memcpy(&value, m_blob.data() + at, 4);

	return value;
}

size_t BbtagMua::Model::Where(int section, int index, size_t stride) const
{
	return m_offset[section] + static_cast<size_t>(index) * stride;
}

int BbtagMua::Model::Count(int section) const
{
	return static_cast<int>(m_count[section]);
}

std::string BbtagMua::Model::Named(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_string.size()))
		return std::string();

	return m_string[index];
}

void BbtagMua::Model::ReadStrings()
{
	m_string.clear();

	const size_t base = m_offset[kString];

	for (int i = 0; i < Count(kStringInfo); ++i)
	{
		const size_t at = Where(kStringInfo, i, 0x10);
		const size_t offset = Dword(at);
		const size_t length = Dword(at + 4);

		std::string text;

		for (size_t k = 0; k < length && base + offset + k < m_blob.size(); ++k)
		{
			const char letter = static_cast<char>(m_blob[base + offset + k]);

			if (letter == 0)
				break;

			text.push_back(letter);
		}

		m_string.push_back(text);
	}
}

void BbtagMua::Model::ReadTextures()
{
	m_texture.clear();

	for (int i = 0; i < Count(kTexture); ++i)
		m_texture.push_back(Named(static_cast<int>(Dword(Where(kTexture, i, 0x10)))));
}

void BbtagMua::Model::ReadMaterials()
{
	std::vector<int> assigned;
	std::vector<Flow> flows;

	for (int i = 0; i < Count(kAssign); ++i)
	{
		const size_t at = Where(kAssign, i, 0x20);
		assigned.push_back(static_cast<int>(Dword(at + 4)));

		const int keys = static_cast<int>(Dword(at + 8));
		const int first = static_cast<int>(Dword(at + 12));

		Flow flow = {};

		if (keys >= 2 && first >= 0 && first + keys <= Count(kUvAnim))
		{
			const size_t one = Where(kUvAnim, first, 0x1c);
			const size_t last = Where(kUvAnim, first + keys - 1, 0x1c);
			const float span = static_cast<float>(Dword(last + 0x10))
				- static_cast<float>(Dword(one + 0x10));

			if (span > 0.0f)
			{
				const float du = (Float(last) - Float(one)) / span;
				const float dv = (Float(last + 4) - Float(one + 4)) / span;

				flow.across = (du < 0.0f ? -du : du) > (dv < 0.0f ? -dv : dv);
				flow.rate = flow.across ? du : dv;
				flow.known = true;
			}
		}

		flows.push_back(flow);
	}

	m_material.clear();
	m_flow.clear();

	for (int i = 0; i < Count(kMaterial); ++i)
	{
		const size_t at = Where(kMaterial, i, 0x50);
		const int count = static_cast<int>(Dword(at));
		const int first = static_cast<int>(Dword(at + 4));

		std::vector<int> textures;
		Flow flow = {};

		for (int k = 0; k < count; ++k)
		{
			const int index = first + k;

			if (index < 0 || index >= static_cast<int>(assigned.size()))
				continue;

			textures.push_back(assigned[index]);

			if (!flow.known && flows[index].known)
				flow = flows[index];
		}

		m_material.push_back(textures);
		m_flow.push_back(flow);
	}
}

void BbtagMua::Model::ReadBones()
{
	m_bone.clear();

	for (int i = 0; i < Count(kBone); ++i)
	{
		const size_t at = Where(kBone, i, 0x130);

		Bone bone;
		bone.name = Named(static_cast<int>(Dword(at)));
		bone.parent = static_cast<int>(Dword(at + 0x3c));

		for (int k = 0; k < 16; ++k)
			bone.matrix[k] = Float(at + 0x48 + k * 4);

		for (int k = 0; k < 3; ++k)
		{
			bone.translation[k] = Float(at + 8 + k * 4);
			bone.rotation[k] = Float(at + 20 + k * 4);
			bone.scale[k] = Float(at + 32 + k * 4);
		}


		bone.frames = static_cast<int>(Dword(at + 0x108));

		for (int k = 0; k < 4; ++k)
			bone.track[k] = static_cast<int>(Dword(at + 0x10c + k * 4));

		m_bone.push_back(bone);
	}

	m_skeleton.clear();

	for (int i = 0; i < Count(kSkeleton); ++i)
	{
		const size_t at = Where(kSkeleton, i, 0x20);
		const Skeleton skeleton = { static_cast<int>(Dword(at)), static_cast<int>(Dword(at + 4)),
			static_cast<int>(Dword(at + 8)) };

		m_skeleton.push_back(skeleton);
	}
}

void BbtagMua::Model::ReadMeshes()
{
	m_part.clear();

	for (int i = 0; i < Count(kPart); ++i)
	{
		const size_t at = Where(kPart, i, 0x20);
		Part part;
		part.material = static_cast<int>(Dword(at));
		part.indices = static_cast<int>(Dword(at + 4));
		part.firstIndex = static_cast<int>(Dword(at + 8));

		m_part.push_back(part);
	}

	m_mesh.clear();

	for (int i = 0; i < Count(kMesh); ++i)
	{
		const size_t at = Where(kMesh, i, 0xc0);
		const int skeleton = static_cast<int>(Float(at));

		Mesh mesh;
		mesh.parts = static_cast<int>(Dword(at + 8));
		mesh.firstPart = static_cast<int>(Dword(at + 12));
		mesh.vertices = static_cast<int>(Dword(at + 16));
		mesh.firstVertex = static_cast<int>(Dword(at + 20));
		mesh.name = Named(static_cast<int>(Dword(at + 0xb4)));
		mesh.skeleton = skeleton;
		mesh.bone = skeleton >= 0 && skeleton < static_cast<int>(m_skeleton.size())
			? m_skeleton[skeleton].firstBone : -1;

		m_mesh.push_back(mesh);
	}
}

bool BbtagMua::Model::Read(const std::vector<uint8_t>& blob)
{
	if (blob.size() < kHeader + kSections * 8 || memcmp(blob.data(), "MUA\0", 4) != 0)
		return false;

	m_blob = blob;

	for (int i = 0; i < kSections; ++i)
	{
		m_offset[i] = Dword(kHeader + i * 8);
		m_count[i] = Dword(kHeader + i * 8 + 4);
	}

	ReadStrings();

	m_script.clear();

	for (int i = 0; i < Count(kScript); ++i)
		m_script.push_back(Named(static_cast<int>(Dword(Where(kScript, i, 0x10)))));

	ReadTextures();
	ReadMaterials();
	ReadBones();
	ReadMeshes();

	return !m_mesh.empty();
}

BbtagMua::Vertex BbtagMua::Model::At(int index) const
{
	const size_t at = m_offset[kVertex] + static_cast<size_t>(index) * kVertexBytes;

	Vertex out = {};

	for (int i = 0; i < 3; ++i)
	{
		out.position[i] = Float(at + i * 4);
		out.normal[i] = Float(at + 12 + i * 4);
	}

	out.uv[0] = Float(at + 36);
	out.uv[1] = Float(at + 40);

	if (at + 56 <= m_blob.size())
	{
		out.colour[0] = m_blob[at + 54];
		out.colour[1] = m_blob[at + 53];
		out.colour[2] = m_blob[at + 52];
		out.colour[3] = m_blob[at + 55];
	}

	out.bone = static_cast<int>(Float(at + 56));

	return out;
}

void BbtagMua::Model::Keys(int track, std::vector<Key>& out) const
{
	out.clear();

	if (track < 0 || track >= Count(kAnimList))
		return;

	const size_t at = Where(kAnimList, track, 0x10);
	const int first = static_cast<int>(Dword(at));
	const int count = static_cast<int>(Dword(at + 4));

	for (int i = 0; i < count; ++i)
	{
		const int index = first + i;

		if (index < 0 || index >= Count(kAnimKey))
			break;

		const size_t row = Where(kAnimKey, index, 0x20);

		Key key = {};

		for (int k = 0; k < 4; ++k)
			key.value[k] = Float(row + k * 4);

		key.frame = static_cast<int>(Dword(row + 0x10));
		out.push_back(key);
	}
}

void BbtagMua::Model::Triangles(const Part& part, std::vector<Triangle>& out) const
{
	out.clear();

	const size_t at = m_offset[kIndex] + static_cast<size_t>(part.firstIndex) * 2;

	if (part.indices < 3 || at + static_cast<size_t>(part.indices) * 2 > m_blob.size())
		return;

	std::vector<int> strip;
	strip.reserve(part.indices);

	for (int i = 0; i < part.indices; ++i)
	{
		uint16_t value = 0;
		memcpy(&value, m_blob.data() + at + i * 2, 2);
		strip.push_back(value);
	}

	for (size_t i = 0; i + 2 < strip.size(); ++i)
	{
		const int a = strip[i];
		const int b = strip[i + 1];
		const int c = strip[i + 2];

		if (a == b || b == c || a == c)
			continue;

		const Triangle triangle = (i % 2) != 0 ? Triangle{ a, c, b } : Triangle{ a, b, c };

		out.push_back(triangle);
	}
}

void BbtagMua::Model::World(int bone, float out[16]) const
{
	Identity(out);

	bool first = true;
	int index = bone;
	int guard = 0;

	while (index >= 0 && index < static_cast<int>(m_bone.size()) && guard++ < 256)
	{
		if (first)
		{
			memcpy(out, m_bone[index].matrix, sizeof(float) * 16);
			first = false;
		}
		else
		{
			float composed[16] = {};
			Multiply(out, m_bone[index].matrix, composed);
			memcpy(out, composed, sizeof(float) * 16);
		}

		index = m_bone[index].parent;
	}
}

void BbtagMua::Multiply(const float left[16], const float right[16], float out[16])
{
	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 4; ++c)
		{
			float total = 0.0f;

			for (int k = 0; k < 4; ++k)
				total += left[r * 4 + k] * right[k * 4 + c];

			out[r * 4 + c] = total;
		}
	}
}

void BbtagMua::Identity(float out[16])
{
	for (int i = 0; i < 16; ++i)
		out[i] = (i % 5) == 0 ? 1.0f : 0.0f;
}

void BbtagMua::Invert(const float matrix[16], float out[16])
{
	const float a[3][3] = {
		{ matrix[0], matrix[1], matrix[2] },
		{ matrix[4], matrix[5], matrix[6] },
		{ matrix[8], matrix[9], matrix[10] },
	};

	const float det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
		- a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
		+ a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);

	if (det > -1e-12f && det < 1e-12f)
	{
		Identity(out);
		return;
	}

	float inverse[3][3] = {};

	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			inverse[c][r] = (a[(r + 1) % 3][(c + 1) % 3] * a[(r + 2) % 3][(c + 2) % 3]
				- a[(r + 1) % 3][(c + 2) % 3] * a[(r + 2) % 3][(c + 1) % 3]) / det;
		}
	}

	for (int i = 0; i < 16; ++i)
		out[i] = 0.0f;

	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
			out[r * 4 + c] = inverse[r][c];
	}

	for (int c = 0; c < 3; ++c)
	{
		float total = 0.0f;

		for (int k = 0; k < 3; ++k)
			total += matrix[12 + k] * inverse[k][c];

		out[12 + c] = -total;
	}

	out[15] = 1.0f;
}
