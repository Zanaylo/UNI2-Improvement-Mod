#pragma once

#include "Game/Stages/FbxExWriter.h"

#include <vector>

class FbxExHierarchy
{
public:
	explicit FbxExHierarchy(const FbxExWriter::Model& model);

	int Parent(int node) const;
	int Frames(int node) const;
	bool Varies(int node) const;

	void Local(int node, int frame, float out[16]) const;
	void World(int node, int frame, float out[16]) const;

private:
	const FbxExWriter::Model& m_model;
	std::vector<int> m_parent;
	std::vector<bool> m_varies;
};
