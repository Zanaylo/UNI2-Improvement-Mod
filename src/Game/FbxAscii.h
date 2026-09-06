#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FbxAscii
{
	struct Node
	{
		std::string name;
		std::vector<std::string> props;
		std::vector<uint32_t> propAt;
		std::vector<double> numbers;
		std::vector<int> children;
	};

	class Tree
	{
	public:
		bool Parse(const uint8_t* data, size_t size);

		const Node& At(int index) const { return m_nodes[index]; }

		int Root() const { return 0; }

		int Find(int parent, const char* name) const;

		void All(int parent, const char* name, std::vector<int>& out) const;

		const std::string& Prop(int index, size_t slot) const;

		bool Empty() const { return m_nodes.size() < 2; }

	private:
		int Add(int parent, const std::string& name);

		std::vector<Node> m_nodes;
		static const std::string s_none;
	};
}
