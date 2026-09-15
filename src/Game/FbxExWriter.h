#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FbxExWriter
{
	constexpr int kVertexFloats = 12;
	constexpr int kMaterialValues = 17;
	constexpr int kMatrixFloats = 16;

	struct Submesh
	{
		int material;
		std::vector<int> indices;
	};

	struct Node
	{
		int type;
		int child;
		int sibling;
		int blendmode;
		float matrix[kMatrixFloats];
		std::vector<float> vertices;
		std::vector<Submesh> submeshes;
	};

	struct Material
	{
		std::string filename;
		int textureIndex;
		float value[kMaterialValues];
	};

	struct Model
	{
		std::vector<std::string> textures;
		std::vector<Material> materials;
		std::vector<Node> nodes;
		std::vector<std::vector<float> > animes;
	};

	Node Branch();

	Node Leaf();

	void Build(const Model& model, std::vector<uint8_t>& out);
}
