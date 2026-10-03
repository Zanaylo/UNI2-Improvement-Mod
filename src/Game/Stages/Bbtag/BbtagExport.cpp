#include "Game/Stages/Bbtag/BbtagExport.h"

#include "Game/Stages/Bbtag/BbtagCamera.h"
#include "Game/Stages/Bbtag/BbtagMua.h"
#include "Game/Stages/Bbtag/BbtagPose.h"
#include "Game/Stages/Bbtag/BbtagRig.h"
#include "Game/Stages/Bbtag/EvbWriter.h"
#include "Game/Stages/Bbtag/IntroCamera.h"
#include "Game/Stages/Bbtag/MmotWriter.h"
#include "Game/Stages/Bbtag/MuaWriter.h"
#include "Game/Stages/Bbtag/PacWriter.h"
#include "Game/Stages/Bbtag/TriangleStrip.h"
#include "Game/Stages/FbxExHierarchy.h"
#include "Game/Stages/FbxExReader.h"
#include "Game/Stages/FbxExWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr float kNeutralScale = 10.0f;
constexpr const char* kSceneScript = "base.evb";
constexpr const char* kSceneBone = "Bone_setting";
constexpr const char* kBonePrefix = "Bone_";
constexpr const char* kTakeSuffix = "_000.mmot";
constexpr const char* kTakeLabel = ".DIG";
constexpr const char* kScriptSuffix = ".evb";
constexpr const char* kModelSuffix = ".MUA";
constexpr const char* kModelFolder = "mdl.pac";
constexpr const char* kScriptFolder = "scr.pac";
constexpr const char* kMotionFolder = "mot.pac";
constexpr const char* kCameraFolder = "cammot.pac";
constexpr const char* kFallbackTexture = "white.dds";

constexpr int kMeshType = 1;
constexpr int kAdditive = 1;
constexpr int kNone = -1;
constexpr int kPosition = 0;
constexpr int kNormal = 3;
constexpr int kColour = 6;
constexpr int kUv = 10;
constexpr int kFloats = FbxExWriter::kVertexFloats;

constexpr float kMoveTolerance = 1e-3f;
constexpr float kTurnTolerance = 1e-5f;
constexpr float kScaleTolerance = 1e-5f;
constexpr int kLongestSpan = 256;
constexpr float kTiny = 1e-12f;

constexpr int32_t kSceneSwitches[] = { 0, 1, 0, 0 };
constexpr int32_t kSceneVector[] = { 0, 900, -1000 };
constexpr int32_t kScenePair[] = { 0, 8000 };
constexpr int32_t kSceneHeight[] = { 750 };

constexpr uint32_t kDdsHeader = 124;
constexpr uint32_t kDdsFlags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000;
constexpr uint32_t kDdsSide = 4;
constexpr uint32_t kDdsBlockBytes = 8;
constexpr uint32_t kDdsPixelFormat = 32;
constexpr uint32_t kDdsFourCc = 0x4;
constexpr uint32_t kDdsTexture = 0x1000;
constexpr size_t kDdsPixelFormatAt = 76;
constexpr size_t kDdsCapsAt = 108;
constexpr size_t kDdsBody = 128;

typedef BbtagExport::File File;

std::string Lowered(const std::string& text)
{
	std::string out = text;
	std::transform(out.begin(), out.end(), out.begin(),
		[](char letter) { return static_cast<char>(tolower(static_cast<unsigned char>(letter))); });

	return out;
}

std::string Leaf(const std::string& path)
{
	const size_t slash = path.find_last_of("\\/");

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

void PutDword(std::vector<uint8_t>& out, size_t at, uint32_t value)
{
	memcpy(out.data() + at, &value, 4);
}

std::vector<uint8_t> WhiteDds()
{
	std::vector<uint8_t> out(kDdsBody + kDdsBlockBytes, 0);
	memcpy(out.data(), "DDS ", 4);
	PutDword(out, 4, kDdsHeader);
	PutDword(out, 8, kDdsFlags);
	PutDword(out, 12, kDdsSide);
	PutDword(out, 16, kDdsSide);
	PutDword(out, 20, kDdsBlockBytes);
	PutDword(out, kDdsPixelFormatAt, kDdsPixelFormat);
	PutDword(out, kDdsPixelFormatAt + 4, kDdsFourCc);
	memcpy(out.data() + kDdsPixelFormatAt + 8, "DXT1", 4);
	PutDword(out, kDdsCapsAt, kDdsTexture);
	out[kDdsBody] = 0xff;
	out[kDdsBody + 1] = 0xff;
	out[kDdsBody + 2] = 0xff;
	out[kDdsBody + 3] = 0xff;

	return out;
}

void Transform(const float point[3], const float world[16], const float place[16], float out[3])
{
	double placed[3] = {};

	for (int c = 0; c < 3; ++c)
	{
		placed[c] = static_cast<double>(point[0]) * world[c] + static_cast<double>(point[1]) * world[4 + c]
			+ static_cast<double>(point[2]) * world[8 + c] + world[12 + c];
	}

	for (int c = 0; c < 3; ++c)
		out[c] = static_cast<float>(placed[0] * place[c] + placed[1] * place[4 + c] + placed[2] * place[8 + c] + place[12 + c]);
}

void Rotate(const float vector[3], const float matrix[16], float out[3])
{
	for (int c = 0; c < 3; ++c)
		out[c] = vector[0] * matrix[c] + vector[1] * matrix[4 + c] + vector[2] * matrix[8 + c];
}

bool Normalise(float vector[3])
{
	const float length = sqrtf(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);

	if (length < kTiny)
		return false;

	for (int k = 0; k < 3; ++k)
		vector[k] /= length;

	return true;
}

void NormalMatrix(const float matrix[16], float out[16])
{
	float inverse[16] = {};
	BbtagMua::Invert(matrix, inverse);
	BbtagMua::Identity(out);

	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
			out[r * 4 + c] = inverse[c * 4 + r];
	}
}

uint8_t Byte(float value)
{
	const float scaled = value * 255.0f + 0.5f;

	return static_cast<uint8_t>(scaled < 0.0f ? 0.0f : (scaled > 255.0f ? 255.0f : scaled));
}

float Dot4(const float left[4], const float right[4])
{
	return left[0] * right[0] + left[1] * right[1] + left[2] * right[2] + left[3] * right[3];
}

void Slerp(const float from[4], const float to[4], float t, float out[4])
{
	float target[4] = { to[0], to[1], to[2], to[3] };
	float cosine = Dot4(from, target);

	if (cosine < 0.0f)
	{
		cosine = -cosine;

		for (float& value : target)
			value = -value;
	}

	float low = 1.0f - t;
	float high = t;

	if (cosine < 0.9999f)
	{
		const float angle = acosf(cosine);
		const float sine = sinf(angle);
		low = sinf((1.0f - t) * angle) / sine;
		high = sinf(t * angle) / sine;
	}

	for (int k = 0; k < 4; ++k)
		out[k] = from[k] * low + target[k] * high;
}

bool Close(const float* left, const float* right, int count, float tolerance)
{
	for (int k = 0; k < count; ++k)
	{
		if (fabsf(left[k] - right[k]) > tolerance)
			return false;
	}

	return true;
}

bool Fits(const std::vector<BbtagPose::Pose>& poses, int from, int to)
{
	const BbtagPose::Pose& a = poses[from];
	const BbtagPose::Pose& b = poses[to];

	for (int k = from + 1; k < to; ++k)
	{
		const float t = static_cast<float>(k - from) / static_cast<float>(to - from);
		const BbtagPose::Pose& actual = poses[k];

		float moved[3] = {};
		float sized[3] = {};

		for (int c = 0; c < 3; ++c)
		{
			moved[c] = a.translation[c] + (b.translation[c] - a.translation[c]) * t;
			sized[c] = a.scale[c] + (b.scale[c] - a.scale[c]) * t;
		}

		float turned[4] = {};
		Slerp(a.turn, b.turn, t, turned);

		if (Dot4(turned, actual.turn) < 0.0f)
		{
			for (float& value : turned)
				value = -value;
		}

		if (!Close(moved, actual.translation, 3, kMoveTolerance) || !Close(sized, actual.scale, 3, kScaleTolerance)
			|| !Close(turned, actual.turn, 4, kTurnTolerance))
		{
			return false;
		}
	}

	return true;
}

std::vector<int> Kept(const std::vector<BbtagPose::Pose>& poses)
{
	std::vector<int> out = { 0 };
	const int last = static_cast<int>(poses.size()) - 1;
	int from = 0;

	while (from < last)
	{
		int to = from + 1;

		while (to < last && to + 1 - from <= kLongestSpan && Fits(poses, from, to + 1))
			++to;

		out.push_back(to);
		from = to;
	}

	return out;
}

void Tangents(std::vector<MuaWriter::Vertex>& vertices, const std::vector<int>& triangles)
{
	std::vector<float> sum(vertices.size() * 3, 0.0f);

	for (size_t i = 0; i + 2 < triangles.size(); i += 3)
	{
		const MuaWriter::Vertex& a = vertices[triangles[i]];
		const MuaWriter::Vertex& b = vertices[triangles[i + 1]];
		const MuaWriter::Vertex& c = vertices[triangles[i + 2]];

		float first[3] = {};
		float second[3] = {};

		for (int k = 0; k < 3; ++k)
		{
			first[k] = b.position[k] - a.position[k];
			second[k] = c.position[k] - a.position[k];
		}

		const float du1 = b.uv[0] - a.uv[0];
		const float dv1 = b.uv[1] - a.uv[1];
		const float du2 = c.uv[0] - a.uv[0];
		const float dv2 = c.uv[1] - a.uv[1];
		const float area = du1 * dv2 - du2 * dv1;

		if (fabsf(area) < kTiny)
			continue;

		for (int corner = 0; corner < 3; ++corner)
		{
			for (int k = 0; k < 3; ++k)
				sum[triangles[i + corner] * 3 + k] += (first[k] * dv2 - second[k] * dv1) / area;
		}
	}

	for (size_t v = 0; v < vertices.size(); ++v)
	{
		MuaWriter::Vertex& vertex = vertices[v];
		float tangent[3] = { sum[v * 3], sum[v * 3 + 1], sum[v * 3 + 2] };
		const float along = tangent[0] * vertex.normal[0] + tangent[1] * vertex.normal[1]
			+ tangent[2] * vertex.normal[2];

		for (int k = 0; k < 3; ++k)
			tangent[k] -= vertex.normal[k] * along;

		if (!Normalise(tangent))
		{
			const float side[3] = { vertex.normal[1], -vertex.normal[0], 0.0f };
			const float up[3] = { 0.0f, vertex.normal[2], -vertex.normal[1] };
			memcpy(tangent, fabsf(vertex.normal[2]) < 0.9f ? side : up, sizeof(tangent));

			if (!Normalise(tangent))
				tangent[0] = 1.0f;
		}

		memcpy(vertex.tangent, tangent, sizeof(tangent));
	}
}

struct Chunk
{
	std::vector<int> sources;
	std::vector<int> map;
	std::vector<std::pair<int, std::vector<int> > > parts;
};

class Exporter
{
public:
	Exporter(const BbtagExport::Source& source, const FbxExWriter::Model& fbx, BbtagExport::Result& out)
		: m_source(source), m_fbx(fbx), m_scene(fbx), m_out(out)
	{
		BbtagExport::Placement(source.framing, m_place);
	}

	bool Run(std::string& error)
	{
		AddScene();

		for (size_t i = 0; i < m_fbx.nodes.size(); ++i)
		{
			const FbxExWriter::Node& node = m_fbx.nodes[i];

			if (node.type == kMeshType && !node.vertices.empty() && !node.submeshes.empty())
				AddNode(static_cast<int>(i));
		}

		if (m_model.meshes.empty())
		{
			error = "the stage has no meshes to export";
			return false;
		}

		Order();
		AddImages();

		MuaWriter::Build(m_model, true, m_out.model);
		MuaWriter::Build(m_model, false, m_out.bare);
		m_out.meshes = static_cast<int>(m_model.meshes.size());
		m_out.turned = m_source.framing.turn != 0.0f;

		return true;
	}

private:
	void AddScene()
	{
		m_model.scripts.push_back(kSceneScript);
		m_model.bones.push_back(MuaWriter::Root(kSceneBone));
		m_model.skeletons.push_back(MuaWriter::Skeleton{ 0, 1, 0, MuaWriter::kBlendUnset,
			MuaWriter::kSceneFlags });

		EvbWriter::Script script;
		script.records.push_back({ EvbWriter::kSceneOpen, {} });
		script.records.push_back({ EvbWriter::kSceneSwitches, Operands(kSceneSwitches) });
		script.records.push_back({ EvbWriter::kSceneVector, Operands(kSceneVector) });
		script.records.push_back({ EvbWriter::kScenePair, Operands(kScenePair) });
		script.records.push_back({ EvbWriter::kSceneHeight, Operands(kSceneHeight) });

		const int tilt = static_cast<int>(lroundf(m_source.framing.tilt));

		if (tilt != 0)
			script.records.push_back({ EvbWriter::kSceneTilt, { tilt, 0, 0 } });

		script.records.push_back({ EvbWriter::kSceneClose, {} });

		File file;
		file.name = kSceneScript;
		EvbWriter::Build(script, file.data);
		m_out.scripts.push_back(file);
	}

	template <size_t N>
	static std::vector<int32_t> Operands(const int32_t (&values)[N])
	{
		return std::vector<int32_t>(values, values + N);
	}

	int TextureOf(int fbxTexture)
	{
		const bool known = fbxTexture >= 0 && fbxTexture < static_cast<int>(m_fbx.textures.size());
		const std::string leaf = known ? Leaf(m_fbx.textures[fbxTexture]) : std::string();
		const std::string name = leaf.empty() ? std::string(kFallbackTexture) : leaf;
		const std::string key = Lowered(name);

		for (size_t i = 0; i < m_model.textures.size(); ++i)
		{
			if (Lowered(m_model.textures[i]) == key)
				return static_cast<int>(i);
		}

		m_model.textures.push_back(name);
		m_model.materials.push_back(MuaWriter::Material{ static_cast<int>(m_model.textures.size()) - 1 });

		return static_cast<int>(m_model.textures.size()) - 1;
	}

	int MaterialOf(int fbxMaterial)
	{
		const bool known = fbxMaterial >= 0 && fbxMaterial < static_cast<int>(m_fbx.materials.size());

		return TextureOf(known ? m_fbx.materials[fbxMaterial].textureIndex : kNone);
	}

	MuaWriter::Vertex Converted(const float* source, const float world[16], const float normals[16], int bone) const
	{
		MuaWriter::Vertex out = {};
		Transform(source + kPosition, world, m_place, out.position);
		Rotate(source + kNormal, normals, out.normal);

		if (!Normalise(out.normal))
			out.normal[1] = 1.0f;

		for (int k = 0; k < 4; ++k)
			out.colour[k] = Byte(source[kColour + k]);

		out.uv[0] = source[kUv];
		out.uv[1] = 1.0f - source[kUv + 1];

		for (int k = 0; k < 3; ++k)
		{
			out.bone[k] = -1.0f;
			out.weight[k] = -1.0f;
		}

		if (bone != kNone)
		{
			out.bone[0] = static_cast<float>(bone);
			out.weight[0] = 1.0f;
		}

		return out;
	}

	std::vector<Chunk> Chunks(const FbxExWriter::Node& node)
	{
		const int count = static_cast<int>(node.vertices.size() / kFloats);
		std::vector<Chunk> out(1);
		out.back().map.assign(count, kNone);

		for (const FbxExWriter::Submesh& submesh : node.submeshes)
		{
			const int material = MaterialOf(submesh.material);
			bool opened = false;

			for (size_t i = 0; i + 2 < submesh.indices.size(); i += 3)
			{
				const int corner[3] = { submesh.indices[i], submesh.indices[i + 2], submesh.indices[i + 1] };

				if (std::any_of(corner, corner + 3, [count](int v) { return v < 0 || v >= count; }))
					continue;

				int needed = 0;

				for (int v : corner)
					needed += out.back().map[v] == kNone ? 1 : 0;

				if (static_cast<int>(out.back().sources.size()) + needed > MuaWriter::kMostVertices)
				{
					out.emplace_back();
					out.back().map.assign(count, kNone);
					opened = false;
				}

				Chunk& chunk = out.back();

				if (!opened)
				{
					chunk.parts.emplace_back(material, std::vector<int>());
					opened = true;
				}

				for (int v : corner)
				{
					if (chunk.map[v] == kNone)
					{
						chunk.map[v] = static_cast<int>(chunk.sources.size());
						chunk.sources.push_back(v);
					}

					chunk.parts.back().second.push_back(chunk.map[v]);
				}
			}
		}

		out.erase(std::remove_if(out.begin(), out.end(),
			[](const Chunk& chunk) { return chunk.sources.empty(); }), out.end());

		return out;
	}

	static std::string JointName(const std::string& name, const BbtagRig::Rig& rig, size_t k)
	{
		return k + 1 == rig.joints.size() ? name : name + "_" + std::to_string(rig.joints[k].node);
	}

	int AddSkeleton(int node, const std::string& name, const BbtagRig::Rig* rig)
	{
		const uint32_t blend = m_fbx.nodes[node].blendmode == kAdditive ? MuaWriter::kBlendAdd
			: MuaWriter::kBlendUnset;
		const int first = static_cast<int>(m_model.bones.size());

		if (rig == nullptr)
		{
			MuaWriter::Bone bone = MuaWriter::Root(name);
			bone.kind = MuaWriter::kMeshBone;
			m_model.bones.push_back(bone);
			m_model.skeletons.push_back(MuaWriter::Skeleton{ first, 1, MuaWriter::kNoScript, blend,
				MuaWriter::kStaticFlags });

			return static_cast<int>(m_model.skeletons.size()) - 1;
		}

		m_model.bones.push_back(MuaWriter::Root(kBonePrefix + name));

		for (size_t k = 0; k < rig->joints.size(); ++k)
			m_model.bones.push_back(MuaWriter::Joint(JointName(name, *rig, k), rig->joints[k].poses.front()));

		const int bones = static_cast<int>(rig->joints.size()) + 1;
		MuaWriter::Link(m_model.bones, first, bones);

		const int script = static_cast<int>(m_model.scripts.size());
		m_model.scripts.push_back(name + kScriptSuffix);
		m_model.skeletons.push_back(MuaWriter::Skeleton{ first, bones, script, blend,
			MuaWriter::kAnimatedFlags });

		return static_cast<int>(m_model.skeletons.size()) - 1;
	}

	void AddNode(int node)
	{
		const FbxExWriter::Node& source = m_fbx.nodes[node];
		const std::vector<Chunk> chunks = Chunks(source);

		if (chunks.empty())
			return;

		BbtagRig::Rig rig;
		const bool moving = BbtagRig::Of(m_scene, node, m_place, rig);
		const int bone = moving ? static_cast<int>(rig.joints.size()) : kNone;

		char label[32] = {};
		sprintf_s(label, "node_%03d", node);
		const std::string name = label;

		float world[16] = {};
		m_scene.World(node, 0, world);

		float matrix[16] = {};
		BbtagMua::Multiply(world, m_place, matrix);

		float normals[16] = {};
		NormalMatrix(matrix, normals);

		const int skeleton = AddSkeleton(node, name, moving ? &rig : nullptr);
		std::vector<std::string> meshNames;

		for (size_t c = 0; c < chunks.size(); ++c)
		{
			const Chunk& chunk = chunks[c];
			std::vector<MuaWriter::Vertex> vertices;
			std::vector<int> every;

			for (int v : chunk.sources)
				vertices.push_back(Converted(source.vertices.data() + v * kFloats, world, normals, bone));

			for (const std::pair<int, std::vector<int> >& part : chunk.parts)
				every.insert(every.end(), part.second.begin(), part.second.end());

			Tangents(vertices, every);

			MuaWriter::Mesh mesh = {};
			mesh.name = c == 0 ? name : name + "_" + std::to_string(c);
			mesh.skeleton = skeleton;
			mesh.partner = skeleton;
			mesh.firstPart = static_cast<int>(m_model.parts.size());
			mesh.parts = static_cast<int>(chunk.parts.size());
			mesh.firstVertex = static_cast<int>(m_model.vertices.size());
			mesh.vertices = static_cast<int>(vertices.size());

			for (const std::pair<int, std::vector<int> >& part : chunk.parts)
			{
				std::vector<uint16_t> strip;
				TriangleStrip::Build(part.second, strip);
				m_model.parts.push_back(MuaWriter::Part{ part.first, static_cast<int>(m_model.indices.size()),
					static_cast<int>(strip.size()) });
				m_model.indices.insert(m_model.indices.end(), strip.begin(), strip.end());
			}

			m_model.vertices.insert(m_model.vertices.end(), vertices.begin(), vertices.end());
			m_model.meshes.push_back(mesh);
			meshNames.push_back(mesh.name);
		}

		if (moving)
			AddTake(name, rig, meshNames);
	}

	void AddTake(const std::string& name, const BbtagRig::Rig& rig, const std::vector<std::string>& meshes)
	{
		MmotWriter::Bone still = {};
		BbtagMua::Identity(still.local);
		BbtagMua::Identity(still.unbind);
		BbtagMua::Identity(still.parentUnbind);

		const BbtagPose::Pose rest = MuaWriter::Root(name).pose;
		MmotWriter::Hold(still, rest, 0);
		MmotWriter::Hold(still, rest, rig.span);

		MmotWriter::Take take;
		take.name = name + kTakeLabel;
		take.root = kBonePrefix + name;
		take.meshes = meshes;
		take.frames = rig.span;
		take.bones.push_back(still);

		float parentWorld[16] = {};
		BbtagMua::Identity(parentWorld);

		for (const BbtagRig::Joint& joint : rig.joints)
		{
			MmotWriter::Bone bone = {};
			BbtagPose::Compose(joint.poses.front(), bone.local);
			BbtagMua::Invert(parentWorld, bone.parentUnbind);

			float world[16] = {};
			BbtagMua::Multiply(bone.local, parentWorld, world);
			BbtagMua::Invert(world, bone.unbind);
			memcpy(parentWorld, world, sizeof(world));

			for (int frame : Kept(joint.poses))
				MmotWriter::Hold(bone, joint.poses[frame], frame);

			take.bones.push_back(bone);
		}

		File motion;
		motion.name = name + kTakeSuffix;
		MmotWriter::Build(take, motion.data);
		m_out.motions.push_back(motion);

		EvbWriter::Script script;
		script.names.push_back(motion.name);
		script.records = { { EvbWriter::kBegin, {} }, { EvbWriter::kWait, { 0 } },
			{ EvbWriter::kPick, { 0 } }, { EvbWriter::kClose, {} }, { EvbWriter::kYield, {} } };

		File file;
		file.name = name + kScriptSuffix;
		EvbWriter::Build(script, file.data);
		m_out.scripts.push_back(file);

		++m_out.animated;
	}

	void Order()
	{
		const float count = static_cast<float>(m_model.meshes.size());

		for (size_t i = 0; i < m_model.meshes.size(); ++i)
			m_model.meshes[i].pivot[2] = count - static_cast<float>(i);
	}

	void AddImages()
	{
		for (const std::string& name : m_model.textures)
		{
			File file;
			file.name = name;

			const bool loaded = m_source.image && m_source.image(name, file.data);

			if (!loaded && Lowered(name) == kFallbackTexture)
				file.data = WhiteDds();

			if (file.data.empty())
			{
				m_out.missing.push_back(name);
				continue;
			}

			if (file.data.size() < 4 || memcmp(file.data.data(), "DDS ", 4) != 0)
				m_out.foreign.push_back(name);

			m_out.images.push_back(file);
		}
	}

	const BbtagExport::Source& m_source;
	const FbxExWriter::Model& m_fbx;
	const FbxExHierarchy m_scene;
	BbtagExport::Result& m_out;
	MuaWriter::Model m_model;
	float m_place[16] = {};
};

bool ByName(const PacWriter::Entry& left, const PacWriter::Entry& right)
{
	return Lowered(left.name) < Lowered(right.name);
}

std::vector<uint8_t> Folder(const std::vector<File>& files)
{
	std::vector<PacWriter::Entry> entries;

	for (const File& file : files)
		entries.push_back(PacWriter::Entry{ file.name, file.data });

	std::sort(entries.begin(), entries.end(), ByName);

	std::vector<uint8_t> out;
	PacWriter::Build(entries, out);

	return out;
}

}

BbtagExport::Framing BbtagExport::Neutral()
{
	Framing out = {};

	for (float& value : out.scale)
		value = kNeutralScale;

	return out;
}

void BbtagExport::Placement(const Framing& framing, float out[16])
{
	const double half = tan(BbtagCamera::kFov * BbtagCamera::kPi / 360.0);
	const float plane = static_cast<float>(BbtagCamera::kEyeDistance * half);

	BbtagMua::Identity(out);
	out[0] = plane * framing.scale[0];
	out[5] = plane * framing.scale[1];
	out[10] = -plane * framing.scale[2];
	out[12] = plane * framing.position[0] / static_cast<float>(BbtagCamera::kAspect);
	out[13] = plane * framing.position[1];
	out[14] = plane * framing.position[2];
}

bool BbtagExport::Convert(const Source& source, Result& out, std::string& error)
{
	out = Result();
	out.stage = source.stage;

	FbxExWriter::Model fbx;

	if (!FbxExReader::Read(source.model, fbx))
	{
		error = "bg.fbx.bin could not be read";
		return false;
	}

	Exporter exporter(source, fbx, out);

	return exporter.Run(error);
}

bool BbtagExport::Package(const Result& result, FbGameFolder::Game game, const std::string& model, Archives& out)
{
	const std::string modelFile = model + kModelSuffix;

	std::vector<PacWriter::Entry> scene;
	scene.push_back(PacWriter::Entry{ kModelFolder, Folder({ File{ modelFile, result.bare } }) });
	scene.push_back(PacWriter::Entry{ kScriptFolder, Folder(result.scripts) });
	scene.push_back(PacWriter::Entry{ kMotionFolder, Folder(result.motions) });

	if (game == FbGameFolder::Game_BBCF)
	{
		const File camera = { IntroCamera::FileName(model), IntroCamera::Still(model) };
		scene.push_back(PacWriter::Entry{ kCameraFolder, Folder({ camera }) });
	}

	Archives plain;
	PacWriter::Build(scene, plain.scene);
	plain.geometry = Folder({ File{ modelFile, result.model } });
	plain.art = Folder(result.images);

	if (game != FbGameFolder::Game_BBCF)
	{
		out = std::move(plain);
		return true;
	}

	return PacWriter::Packed(plain.scene, out.scene) && PacWriter::Packed(plain.geometry, out.geometry)
		&& PacWriter::Packed(plain.art, out.art);
}
