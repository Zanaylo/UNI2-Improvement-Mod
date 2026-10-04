#include "Game/Stages/Bbtag/MuaWriter.h"

#include "Game/Stages/Bbtag/BbtagMua.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>

namespace {

constexpr uint32_t kVersion = 0x3ee;
constexpr int kSections = 17;
constexpr size_t kTable = 0x20;
constexpr size_t kData = kTable + kSections * 8;

constexpr int kSkeleton = 0;
constexpr int kBone = 1;
constexpr int kMesh = 2;
constexpr int kPart = 3;
constexpr int kMaterial = 4;
constexpr int kAssign = 5;
constexpr int kTexture = 6;
constexpr int kScript = 11;
constexpr int kOrder = 12;
constexpr int kVertex = 13;
constexpr int kIndex = 14;
constexpr int kStringInfo = 15;
constexpr int kString = 16;

constexpr size_t kStride[kSections] = { 0x20, 0x130, 0xc0, 0x20, 0x50, 0x20, 0x10, 0x1c, 0x10,
	0x20, 0x20, 0x10, 0x20, 0x50, 2, 0x10, 1 };

constexpr int kBaseLayer = 1;
constexpr float kAmbient = 0.588f;
constexpr float kDiffuse = 0.588f;
constexpr float kSpecular = 0.9f;
constexpr float kPower = 0.1f;
constexpr float kShown = 1.0f;
constexpr int kNone = -1;
constexpr int kCorners = 8;

class Sink
{
public:
	explicit Sink(std::vector<uint8_t>& out) : m_out(out) {}

	size_t Size() const { return m_out.size(); }

	void Dword(uint32_t value)
	{
		const uint8_t bytes[4] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
			static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24) };
		m_out.insert(m_out.end(), bytes, bytes + 4);
	}

	void Int(int value) { Dword(static_cast<uint32_t>(value)); }

	void Float(float value)
	{
		uint32_t bits = 0;
		memcpy(&bits, &value, 4);
		Dword(bits);
	}

	void Floats(const float* values, size_t count)
	{
		for (size_t i = 0; i < count; ++i)
			Float(values[i]);
	}

	void Word(uint16_t value)
	{
		m_out.push_back(static_cast<uint8_t>(value));
		m_out.push_back(static_cast<uint8_t>(value >> 8));
	}

	void Byte(uint8_t value) { m_out.push_back(value); }

	void Pad(size_t start, size_t stride)
	{
		while (m_out.size() < start + stride)
			m_out.push_back(0);
	}

private:
	std::vector<uint8_t>& m_out;
};

class Strings
{
public:
	int Of(const std::string& text)
	{
		const std::map<std::string, int>::const_iterator known = m_index.find(text);

		if (known != m_index.end())
			return known->second;

		const int index = static_cast<int>(m_text.size());
		m_index[text] = index;
		m_text.push_back(text);

		return index;
	}

	const std::vector<std::string>& All() const { return m_text; }

private:
	std::map<std::string, int> m_index;
	std::vector<std::string> m_text;
};

struct Bounds
{
	float centre[3];
	float radius;
	float low[3];
	float high[3];
};

Bounds BoundsOf(const MuaWriter::Model& model, const MuaWriter::Mesh& mesh)
{
	Bounds out = {};

	if (mesh.vertices <= 0)
		return out;

	for (int k = 0; k < 3; ++k)
	{
		out.low[k] = model.vertices[mesh.firstVertex].position[k];
		out.high[k] = out.low[k];
	}

	for (int v = 0; v < mesh.vertices; ++v)
	{
		const float* position = model.vertices[mesh.firstVertex + v].position;

		for (int k = 0; k < 3; ++k)
		{
			out.low[k] = std::min(out.low[k], position[k]);
			out.high[k] = std::max(out.high[k], position[k]);
		}
	}

	for (int k = 0; k < 3; ++k)
		out.centre[k] = (out.low[k] + out.high[k]) * 0.5f;

	float farthest = 0.0f;

	for (int v = 0; v < mesh.vertices; ++v)
	{
		const float* position = model.vertices[mesh.firstVertex + v].position;
		float distance = 0.0f;

		for (int k = 0; k < 3; ++k)
			distance += (position[k] - out.centre[k]) * (position[k] - out.centre[k]);

		farthest = std::max(farthest, distance);
	}

	out.radius = sqrtf(farthest);

	return out;
}

void PutIdentity(Sink& sink)
{
	float identity[16] = {};
	BbtagMua::Identity(identity);
	sink.Floats(identity, 16);
}

void PutSkeletons(Sink& sink, const MuaWriter::Model& model)
{
	for (const MuaWriter::Skeleton& skeleton : model.skeletons)
	{
		const size_t start = sink.Size();
		sink.Int(skeleton.firstBone);
		sink.Int(skeleton.bones);
		sink.Int(skeleton.script);
		sink.Dword(skeleton.blend);
		sink.Dword(skeleton.flags);
		sink.Pad(start, kStride[kSkeleton]);
	}
}

void PutBones(Sink& sink, const MuaWriter::Model& model, Strings& strings)
{
	for (const MuaWriter::Bone& bone : model.bones)
	{
		const size_t start = sink.Size();
		const bool joint = bone.kind == MuaWriter::kJointBone;

		sink.Int(strings.Of(bone.name));
		sink.Int(bone.kind);
		sink.Floats(bone.pose.translation, 3);
		sink.Floats(bone.pose.rotation, 3);
		sink.Floats(bone.pose.scale, 3);
		sink.Int(0);
		sink.Int(joint ? 1 : 0);
		sink.Int(0);
		sink.Int(joint ? bone.index : 0);
		sink.Int(bone.parent);
		sink.Int(bone.child);
		sink.Int(bone.sibling);
		sink.Floats(bone.matrix, 16);
		sink.Floats(bone.unbind, 16);
		sink.Floats(bone.parentUnbind, 16);
		sink.Pad(start, kStride[kBone]);
	}
}

void PutMeshes(Sink& sink, const MuaWriter::Model& model, Strings& strings)
{
	for (const MuaWriter::Mesh& mesh : model.meshes)
	{
		const size_t start = sink.Size();
		const Bounds bounds = BoundsOf(model, mesh);

		sink.Float(static_cast<float>(mesh.skeleton));
		sink.Int(0);
		sink.Int(mesh.parts);
		sink.Int(mesh.firstPart);
		sink.Int(mesh.vertices);
		sink.Int(mesh.firstVertex);
		sink.Floats(bounds.centre, 3);
		sink.Float(bounds.radius);

		for (int axis = 0; axis < 3; ++axis)
		{
			for (int corner = 0; corner < kCorners; ++corner)
			{
				const bool high = axis == 0 ? ((corner + 1) / 2) % 2 == 0
					: axis == 1 ? (corner % 4) < 2 : corner < 4;

				sink.Float(high ? bounds.high[axis] : bounds.low[axis]);
			}
		}

		const float turn[3] = {};
		const float size[3] = { 1.0f, 1.0f, 1.0f };
		sink.Floats(mesh.pivot, 3);
		sink.Floats(turn, 3);
		sink.Floats(size, 3);
		sink.Float(kShown);
		sink.Int(0);
		sink.Int(strings.Of(mesh.name));
		sink.Int(mesh.partner);
		sink.Pad(start, kStride[kMesh]);
	}
}

void PutParts(Sink& sink, const MuaWriter::Model& model)
{
	for (const MuaWriter::Part& part : model.parts)
	{
		const size_t start = sink.Size();
		sink.Int(part.material);
		sink.Int(part.indices);
		sink.Int(part.firstIndex);
		sink.Pad(start, kStride[kPart]);
	}
}

void PutMaterials(Sink& sink, const MuaWriter::Model& model)
{
	const float ambient[3] = { kAmbient, kAmbient, kAmbient };
	const float diffuse[3] = { kDiffuse, kDiffuse, kDiffuse };
	const float specular[3] = { kSpecular, kSpecular, kSpecular };

	for (size_t i = 0; i < model.materials.size(); ++i)
	{
		const size_t start = sink.Size();
		sink.Int(1);
		sink.Int(static_cast<int>(i));
		sink.Int(0);
		sink.Floats(ambient, 3);
		sink.Int(0);
		sink.Floats(diffuse, 3);
		sink.Int(0);
		sink.Floats(specular, 3);
		sink.Float(kPower);
		sink.Pad(start, kStride[kMaterial]);
	}
}

void PutAssigns(Sink& sink, const MuaWriter::Model& model)
{
	for (const MuaWriter::Material& material : model.materials)
	{
		const size_t start = sink.Size();
		sink.Int(kBaseLayer);
		sink.Int(material.texture);
		sink.Pad(start, kStride[kAssign]);
	}
}

void PutNames(Sink& sink, const std::vector<std::string>& names, Strings& strings, size_t stride)
{
	for (const std::string& name : names)
	{
		const size_t start = sink.Size();
		sink.Int(strings.Of(name));
		sink.Pad(start, stride);
	}
}

void PutOrder(Sink& sink, const MuaWriter::Model& model)
{
	for (size_t i = 0; i < model.meshes.size(); ++i)
	{
		for (int part = 0; part < model.meshes[i].parts; ++part)
		{
			const size_t start = sink.Size();
			sink.Int(static_cast<int>(i));
			sink.Int(part);
			sink.Pad(start, kStride[kOrder]);
		}
	}
}

void PutVertices(Sink& sink, const MuaWriter::Model& model)
{
	const float second[2] = {};

	for (const MuaWriter::Vertex& vertex : model.vertices)
	{
		sink.Floats(vertex.position, 3);
		sink.Floats(vertex.normal, 3);
		sink.Floats(vertex.tangent, 3);
		sink.Floats(vertex.uv, 2);
		sink.Floats(second, 2);
		sink.Byte(vertex.colour[2]);
		sink.Byte(vertex.colour[1]);
		sink.Byte(vertex.colour[0]);
		sink.Byte(vertex.colour[3]);
		sink.Floats(vertex.bone, 3);
		sink.Floats(vertex.weight, 3);
	}
}

void PutStringInfo(Sink& sink, const Strings& strings)
{
	size_t offset = 0;

	for (const std::string& text : strings.All())
	{
		const size_t start = sink.Size();
		sink.Int(static_cast<int>(offset));
		sink.Int(static_cast<int>(text.size()));
		sink.Pad(start, kStride[kStringInfo]);
		offset += text.size() + 1;
	}
}

void PutStrings(Sink& sink, const Strings& strings)
{
	for (const std::string& text : strings.All())
	{
		for (char letter : text)
			sink.Byte(static_cast<uint8_t>(letter));

		sink.Byte(0);
	}
}

void Intern(const MuaWriter::Model& model, Strings& strings)
{
	for (const MuaWriter::Bone& bone : model.bones)
		strings.Of(bone.name);

	for (const MuaWriter::Mesh& mesh : model.meshes)
		strings.Of(mesh.name);

	for (const std::string& texture : model.textures)
		strings.Of(texture);

	for (const std::string& script : model.scripts)
		strings.Of(script);
}

void Matrices(MuaWriter::Bone& bone, const float parentWorld[16])
{
	float world[16] = {};
	BbtagMua::Multiply(bone.matrix, parentWorld, world);
	BbtagMua::Invert(world, bone.unbind);
	BbtagMua::Invert(parentWorld, bone.parentUnbind);
}

}

MuaWriter::Bone MuaWriter::Root(const std::string& name)
{
	Bone out = {};
	out.name = name;
	out.kind = kRootBone;
	out.parent = kNone;
	out.child = kNone;
	out.sibling = kNone;
	out.pose.scale[0] = out.pose.scale[1] = out.pose.scale[2] = 1.0f;
	BbtagPose::Turned(out.pose);
	BbtagMua::Identity(out.matrix);
	BbtagMua::Identity(out.unbind);
	BbtagMua::Identity(out.parentUnbind);

	return out;
}

MuaWriter::Bone MuaWriter::Joint(const std::string& name, const BbtagPose::Pose& pose)
{
	Bone out = Root(name);
	out.kind = kJointBone;
	out.pose = pose;
	BbtagPose::Compose(pose, out.matrix);

	return out;
}

void MuaWriter::Link(std::vector<Bone>& bones, int first, int count)
{
	std::vector<int> parents;

	for (int i = 0; i < count; ++i)
		parents.push_back(i - 1);

	Tree(bones, first, parents);
}

void MuaWriter::Tree(std::vector<Bone>& bones, int first, const std::vector<int>& parents)
{
	const int count = static_cast<int>(parents.size());

	if (first < 0 || count < 1 || first + count > static_cast<int>(bones.size()))
		return;

	std::vector<std::array<float, 16> > worlds(parents.size());

	for (int i = 0; i < count; ++i)
	{
		Bone& bone = bones[first + i];
		bone.index = i;
		bone.parent = parents[i] < i ? parents[i] : kNone;
		bone.child = kNone;
		bone.sibling = kNone;

		float parentWorld[16] = {};
		BbtagMua::Identity(parentWorld);

		if (bone.parent != kNone)
			memcpy(parentWorld, worlds[bone.parent].data(), sizeof(parentWorld));

		Matrices(bone, parentWorld);
		BbtagMua::Multiply(bone.matrix, parentWorld, worlds[i].data());
	}

	for (int i = count - 1; i > 0; --i)
	{
		const int parent = bones[first + i].parent;

		if (parent == kNone)
			continue;

		bones[first + i].sibling = bones[first + parent].child;
		bones[first + parent].child = i;
	}
}

void MuaWriter::Build(const Model& model, bool withGeometry, std::vector<uint8_t>& out)
{
	out.clear();

	Strings strings;
	Intern(model, strings);

	const uint32_t counts[kSections] = {
		static_cast<uint32_t>(model.skeletons.size()), static_cast<uint32_t>(model.bones.size()),
		static_cast<uint32_t>(model.meshes.size()), static_cast<uint32_t>(model.parts.size()),
		static_cast<uint32_t>(model.materials.size()), static_cast<uint32_t>(model.materials.size()),
		static_cast<uint32_t>(model.textures.size()), 0, 0, 0, 0,
		static_cast<uint32_t>(model.scripts.size()), static_cast<uint32_t>(model.parts.size()),
		withGeometry ? static_cast<uint32_t>(model.vertices.size()) : 0,
		withGeometry ? static_cast<uint32_t>(model.indices.size()) : 0,
		static_cast<uint32_t>(strings.All().size()), 0,
	};

	std::vector<uint8_t> body;
	Sink sink(body);
	uint32_t offsets[kSections] = {};

	const auto mark = [&](int section) { offsets[section] = static_cast<uint32_t>(kData + sink.Size()); };

	mark(kSkeleton);
	PutSkeletons(sink, model);
	mark(kBone);
	PutBones(sink, model, strings);
	mark(kMesh);
	PutMeshes(sink, model, strings);
	mark(kPart);
	PutParts(sink, model);
	mark(kMaterial);
	PutMaterials(sink, model);
	mark(kAssign);
	PutAssigns(sink, model);
	mark(kTexture);
	PutNames(sink, model.textures, strings, kStride[kTexture]);

	for (int empty = kTexture + 1; empty < kScript; ++empty)
		mark(empty);

	mark(kScript);
	PutNames(sink, model.scripts, strings, kStride[kScript]);
	mark(kOrder);
	PutOrder(sink, model);
	mark(kVertex);

	if (withGeometry)
		PutVertices(sink, model);

	mark(kIndex);

	if (withGeometry)
	{
		for (uint16_t index : model.indices)
			sink.Word(index);
	}

	mark(kStringInfo);
	PutStringInfo(sink, strings);
	mark(kString);
	PutStrings(sink, strings);

	uint32_t stringBytes = 0;

	for (const std::string& text : strings.All())
		stringBytes += static_cast<uint32_t>(text.size() + 1);

	Sink head(out);
	head.Byte('M');
	head.Byte('U');
	head.Byte('A');
	head.Byte(0);
	head.Dword(kVersion);
	head.Dword(kSections);
	head.Pad(0, kTable);

	for (int i = 0; i < kSections; ++i)
	{
		head.Dword(offsets[i]);
		head.Dword(i == kString ? stringBytes : counts[i]);
	}

	out.insert(out.end(), body.begin(), body.end());
}
