#pragma once

#include "Game/Stages/Bbtag/BbtagPose.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MuaWriter
{
	constexpr int kNoScript = -1;
	constexpr uint32_t kBlendUnset = 0x7fffffff;
	constexpr uint32_t kBlendAdd = 2;
	constexpr uint32_t kSceneFlags = 0x10000100;
	constexpr uint32_t kStaticFlags = 0x20000100;
	constexpr uint32_t kAnimatedFlags = 0x10000140;
	constexpr int kRootBone = 5;
	constexpr int kMeshBone = 0x84;
	constexpr int kJointBone = 1;
	constexpr int kMostVertices = 65535;

	struct Bone
	{
		std::string name;
		int kind;
		int parent;
		int child;
		int sibling;
		int index;
		BbtagPose::Pose pose;
		float matrix[16];
		float unbind[16];
		float parentUnbind[16];
	};

	struct Skeleton
	{
		int firstBone;
		int bones;
		int script;
		uint32_t blend;
		uint32_t flags;
	};

	struct Vertex
	{
		float position[3];
		float normal[3];
		float tangent[3];
		float uv[2];
		uint8_t colour[4];
		float bone[3];
		float weight[3];
	};

	struct Part
	{
		int material;
		int firstIndex;
		int indices;
	};

	struct Mesh
	{
		std::string name;
		int skeleton;
		int partner;
		int firstPart;
		int parts;
		int firstVertex;
		int vertices;
		float pivot[3];
	};

	struct Material
	{
		int texture;
	};

	struct Model
	{
		std::vector<std::string> textures;
		std::vector<Material> materials;
		std::vector<std::string> scripts;
		std::vector<Bone> bones;
		std::vector<Skeleton> skeletons;
		std::vector<Mesh> meshes;
		std::vector<Part> parts;
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;
	};

	Bone Root(const std::string& name);

	Bone Joint(const std::string& name, const BbtagPose::Pose& pose);

	void Link(std::vector<Bone>& bones, int first, int count);

	void Build(const Model& model, bool withGeometry, std::vector<uint8_t>& out);
}
