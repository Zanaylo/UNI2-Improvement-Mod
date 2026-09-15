#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BbtagMua
{
	struct Vertex
	{
		float position[3];
		float normal[3];
		float uv[2];
		uint8_t colour[4];
		int bone;
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
		int bone;
		int skeleton;
		int firstVertex;
		int vertices;
		int firstPart;
		int parts;
	};

	struct Bone
	{
		std::string name;
		int parent;
		float matrix[16];
		float translation[3];
		float rotation[3];
		float scale[3];
		int frames;
		int track[4];
	};

	struct Skeleton
	{
		int firstBone;
		int bones;
		int script;
	};

	struct Flow
	{
		float rate;
		bool across;
		bool known;
	};

	struct Key
	{
		float value[4];
		int frame;
	};

	struct Triangle
	{
		int a;
		int b;
		int c;
	};

	class Model
	{
	public:
		bool Read(const std::vector<uint8_t>& blob);

		const std::vector<std::string>& Textures() const { return m_texture; }
		const std::vector<std::vector<int> >& Materials() const { return m_material; }
		const std::vector<Mesh>& Meshes() const { return m_mesh; }
		const std::vector<Part>& Parts() const { return m_part; }
		const std::vector<Bone>& Bones() const { return m_bone; }
		const std::vector<Skeleton>& Skeletons() const { return m_skeleton; }
		const std::vector<std::string>& Scripts() const { return m_script; }
		const std::vector<Flow>& Flows() const { return m_flow; }

		Vertex At(int index) const;
		void Triangles(const Part& part, std::vector<Triangle>& out) const;
		void Keys(int track, std::vector<Key>& out) const;
		void World(int bone, float out[16]) const;

	private:
		uint32_t Dword(size_t at) const;
		float Float(size_t at) const;
		size_t Where(int section, int index, size_t stride) const;
		int Count(int section) const;
		std::string Named(int index) const;

		void ReadStrings();
		void ReadTextures();
		void ReadMaterials();
		void ReadBones();
		void ReadMeshes();

		std::vector<uint8_t> m_blob;
		uint32_t m_offset[17] = {};
		uint32_t m_count[17] = {};
		std::vector<std::string> m_string;
		std::vector<std::string> m_texture;
		std::vector<std::vector<int> > m_material;
		std::vector<Flow> m_flow;
		std::vector<Bone> m_bone;
		std::vector<Skeleton> m_skeleton;
		std::vector<std::string> m_script;
		std::vector<Mesh> m_mesh;
		std::vector<Part> m_part;
	};

	void Multiply(const float left[16], const float right[16], float out[16]);
	void Invert(const float matrix[16], float out[16]);
	void Identity(float out[16]);
}
