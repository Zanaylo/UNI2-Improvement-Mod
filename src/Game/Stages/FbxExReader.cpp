#include "Game/Stages/FbxExReader.h"

#include <cstring>

namespace {

constexpr size_t kHeaderBytes = 16;
constexpr size_t kBlockHeaderBytes = 8;
constexpr size_t kNameBytes = 128;
constexpr size_t kNodeHeaderBytes = 16;
constexpr int kMeshType = 1;
constexpr int kBlocks = 4;
const char kMagic[8] = { 'f', 'b', 'x', 'e', 'x', 0, 0, 0 };

class Cursor
{
public:
	Cursor(const std::vector<uint8_t>& blob, size_t at, size_t end)
		: m_blob(blob), m_at(at), m_end(end)
	{
	}

	bool Has(size_t bytes) const { return m_at <= m_end && bytes <= m_end - m_at; }
	size_t At() const { return m_at; }
	bool Done() const { return m_at == m_end; }
	void Skip(size_t to) { m_at = to; }

	bool Int(int& out)
	{
		if (!Has(4))
			return false;

		memcpy(&out, m_blob.data() + m_at, 4);
		m_at += 4;

		return true;
	}

	bool Floats(size_t count, std::vector<float>& out)
	{
		if (count > (m_end - m_at) / 4)
			return false;

		const size_t first = out.size();
		out.resize(first + count);

		if (count > 0)
			memcpy(out.data() + first, m_blob.data() + m_at, count * 4);

		m_at += count * 4;

		return true;
	}

	bool Floats(size_t count, float* out)
	{
		if (count > (m_end - m_at) / 4)
			return false;

		memcpy(out, m_blob.data() + m_at, count * 4);
		m_at += count * 4;

		return true;
	}

	bool Ints(size_t count, std::vector<int>& out)
	{
		if (count > (m_end - m_at) / 4)
			return false;

		out.resize(count);

		if (count > 0)
			memcpy(out.data(), m_blob.data() + m_at, count * 4);

		m_at += count * 4;

		return true;
	}

	bool Name(std::string& out)
	{
		if (!Has(kNameBytes))
			return false;

		const char* text = reinterpret_cast<const char*>(m_blob.data() + m_at);
		out.assign(text, strnlen(text, kNameBytes));
		m_at += kNameBytes;

		return true;
	}

private:
	const std::vector<uint8_t>& m_blob;
	size_t m_at;
	size_t m_end;
};

struct Block
{
	size_t body;
	size_t end;
	int count;
};

bool Blocks(const std::vector<uint8_t>& blob, Block out[kBlocks])
{
	if (blob.size() < kHeaderBytes || memcmp(blob.data(), kMagic, sizeof(kMagic)) != 0)
		return false;

	size_t at = kHeaderBytes;

	for (int i = 0; i < kBlocks; ++i)
	{
		if (blob.size() - at < kBlockHeaderBytes)
			return false;

		uint32_t size = 0;
		int count = 0;
		memcpy(&size, blob.data() + at, 4);
		memcpy(&count, blob.data() + at + 4, 4);

		if (size < kBlockHeaderBytes || size > blob.size() - at || count < 0)
			return false;

		out[i] = Block{ at + kBlockHeaderBytes, at + size, count };
		at += size;
	}

	return at == blob.size();
}

bool ReadTextures(const std::vector<uint8_t>& blob, const Block& block, FbxExWriter::Model& out)
{
	Cursor cursor(blob, block.body, block.end);

	for (int i = 0; i < block.count; ++i)
	{
		std::string name;

		if (!cursor.Name(name))
			return false;

		out.textures.push_back(name);
	}

	return cursor.Done();
}

bool ReadMaterials(const std::vector<uint8_t>& blob, const Block& block, FbxExWriter::Model& out)
{
	Cursor cursor(blob, block.body, block.end);

	for (int i = 0; i < block.count; ++i)
	{
		FbxExWriter::Material material = {};
		int index = 0;

		if (!cursor.Name(material.filename) || !cursor.Int(index) || !cursor.Int(material.textureIndex)
			|| !cursor.Floats(FbxExWriter::kMaterialValues, material.value))
		{
			return false;
		}

		out.materials.push_back(material);
	}

	return cursor.Done();
}

bool ReadMesh(Cursor& cursor, FbxExWriter::Node& node)
{
	int vertices = 0;

	if (!cursor.Int(node.blendmode) || !cursor.Int(node.alpha)
		|| !cursor.Floats(FbxExWriter::kMatrixFloats, node.matrix) || !cursor.Int(vertices)
		|| vertices < 0 || !cursor.Floats(static_cast<size_t>(vertices) * FbxExWriter::kVertexFloats,
			node.vertices))
	{
		return false;
	}

	int submeshes = 0;

	if (!cursor.Int(submeshes) || submeshes < 0)
		return false;

	for (int i = 0; i < submeshes; ++i)
	{
		FbxExWriter::Submesh submesh;
		int indices = 0;

		if (!cursor.Int(submesh.material) || !cursor.Int(indices) || indices < 0
			|| !cursor.Ints(static_cast<size_t>(indices), submesh.indices))
		{
			return false;
		}

		node.submeshes.push_back(submesh);
	}

	return true;
}

bool ReadNodes(const std::vector<uint8_t>& blob, const Block& block, FbxExWriter::Model& out)
{
	Cursor cursor(blob, block.body, block.end);

	for (int i = 0; i < block.count; ++i)
	{
		const size_t start = cursor.At();
		int size = 0;
		FbxExWriter::Node node = FbxExWriter::Branch();

		if (!cursor.Int(size) || !cursor.Int(node.type) || !cursor.Int(node.child)
			|| !cursor.Int(node.sibling) || size < static_cast<int>(kNodeHeaderBytes)
			|| static_cast<size_t>(size) > block.end - start)
		{
			return false;
		}

		Cursor payload(blob, start + kNodeHeaderBytes, start + static_cast<size_t>(size));

		if (node.type == kMeshType && (!ReadMesh(payload, node) || !payload.Done()))
			return false;

		out.nodes.push_back(node);
		cursor.Skip(start + static_cast<size_t>(size));
	}

	return cursor.Done();
}

bool ReadAnimes(const std::vector<uint8_t>& blob, const Block& block, FbxExWriter::Model& out)
{
	Cursor cursor(blob, block.body, block.end);

	for (int i = 0; i < block.count; ++i)
	{
		int frames = 0;
		std::vector<float> track;

		if (!cursor.Int(frames) || frames < 0
			|| !cursor.Floats(static_cast<size_t>(frames) * FbxExWriter::kMatrixFloats, track))
		{
			return false;
		}

		out.animes.push_back(track);
	}

	return cursor.Done();
}

}

bool FbxExReader::Read(const std::vector<uint8_t>& blob, FbxExWriter::Model& out)
{
	out = FbxExWriter::Model();

	Block blocks[kBlocks] = {};

	if (!Blocks(blob, blocks))
		return false;

	return ReadTextures(blob, blocks[0], out) && ReadMaterials(blob, blocks[1], out)
		&& ReadNodes(blob, blocks[2], out) && ReadAnimes(blob, blocks[3], out);
}
