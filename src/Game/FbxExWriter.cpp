#include "Game/FbxExWriter.h"

#include <cstring>

namespace {

constexpr size_t kNameBytes = 128;
constexpr int kNodePrefix = 16;

void PutDword(std::vector<uint8_t>& out, uint32_t value)
{
	out.push_back(static_cast<uint8_t>(value));
	out.push_back(static_cast<uint8_t>(value >> 8));
	out.push_back(static_cast<uint8_t>(value >> 16));
	out.push_back(static_cast<uint8_t>(value >> 24));
}

void PutInt(std::vector<uint8_t>& out, int value)
{
	PutDword(out, static_cast<uint32_t>(value));
}

void PutFloat(std::vector<uint8_t>& out, float value)
{
	uint32_t bits = 0;
	memcpy(&bits, &value, 4);
	PutDword(out, bits);
}

void PutName(std::vector<uint8_t>& out, const std::string& text)
{
	for (size_t i = 0; i < kNameBytes; ++i)
		out.push_back(i < text.size() ? static_cast<uint8_t>(text[i]) : 0);
}

void PutBlock(std::vector<uint8_t>& out, int count, const std::vector<uint8_t>& body)
{
	PutDword(out, static_cast<uint32_t>(body.size() + 8));
	PutInt(out, count);
	out.insert(out.end(), body.begin(), body.end());
}

void PutMesh(std::vector<uint8_t>& out, const FbxExWriter::Node& node)
{
	PutInt(out, node.blendmode);
	PutInt(out, 0);

	for (float value : node.matrix)
		PutFloat(out, value);

	PutInt(out, static_cast<int>(node.vertices.size() / FbxExWriter::kVertexFloats));

	for (float value : node.vertices)
		PutFloat(out, value);

	PutInt(out, static_cast<int>(node.submeshes.size()));

	for (const FbxExWriter::Submesh& submesh : node.submeshes)
	{
		PutInt(out, submesh.material);
		PutInt(out, static_cast<int>(submesh.indices.size()));

		for (int index : submesh.indices)
			PutInt(out, index);
	}
}

}

FbxExWriter::Node FbxExWriter::Branch()
{
	Node out = {};
	out.type = 0;
	out.child = -1;
	out.sibling = -1;

	for (int i = 0; i < kMatrixFloats; ++i)
		out.matrix[i] = (i % 5) == 0 ? 1.0f : 0.0f;

	return out;
}

FbxExWriter::Node FbxExWriter::Leaf()
{
	Node out = Branch();
	out.type = 1;

	return out;
}

void FbxExWriter::Build(const Model& model, std::vector<uint8_t>& out)
{
	out.clear();

	const uint8_t magic[8] = { 'f', 'b', 'x', 'e', 'x', 0, 0, 0 };
	out.insert(out.end(), magic, magic + 8);
	PutDword(out, 0);
	PutDword(out, 0);

	std::vector<uint8_t> body;

	for (const std::string& name : model.textures)
		PutName(body, name);

	PutBlock(out, static_cast<int>(model.textures.size()), body);

	body.clear();

	for (size_t i = 0; i < model.materials.size(); ++i)
	{
		PutName(body, model.materials[i].filename);
		PutInt(body, static_cast<int>(i));
		PutInt(body, model.materials[i].textureIndex);

		for (float value : model.materials[i].value)
			PutFloat(body, value);
	}

	PutBlock(out, static_cast<int>(model.materials.size()), body);

	body.clear();

	for (const Node& node : model.nodes)
	{
		std::vector<uint8_t> payload;

		if (node.type == 1)
			PutMesh(payload, node);

		PutInt(body, static_cast<int>(payload.size()) + kNodePrefix);
		PutInt(body, node.type);
		PutInt(body, node.child);
		PutInt(body, node.sibling);
		body.insert(body.end(), payload.begin(), payload.end());
	}

	PutBlock(out, static_cast<int>(model.nodes.size()), body);

	body.clear();

	for (const std::vector<float>& track : model.animes)
	{
		PutInt(body, static_cast<int>(track.size() / kMatrixFloats));

		for (float value : track)
			PutFloat(body, value);
	}

	PutBlock(out, static_cast<int>(model.animes.size()), body);
}
