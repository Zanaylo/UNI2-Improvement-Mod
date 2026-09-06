#include "Game/FbxAscii.h"

#include <cstdlib>

const std::string FbxAscii::Tree::s_none;

namespace {

bool IsNameByte(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
		|| c == '_' || c == '-';
}

bool IsSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r';
}

bool Numeric(const std::string& text, double& out)
{
	if (text.empty())
		return false;

	char* end = nullptr;
	const double value = strtod(text.c_str(), &end);

	if (end == text.c_str() || *end != 0)
		return false;

	out = value;
	return true;
}

}

int FbxAscii::Tree::Add(int parent, const std::string& name)
{
	m_nodes.push_back(Node());
	const int index = static_cast<int>(m_nodes.size()) - 1;
	m_nodes[index].name = name;

	if (parent >= 0)
		m_nodes[parent].children.push_back(index);

	return index;
}

int FbxAscii::Tree::Find(int parent, const char* name) const
{
	if (parent < 0 || parent >= static_cast<int>(m_nodes.size()))
		return -1;

	for (int child : m_nodes[parent].children)
	{
		if (m_nodes[child].name == name)
			return child;
	}

	return -1;
}

void FbxAscii::Tree::All(int parent, const char* name, std::vector<int>& out) const
{
	out.clear();

	if (parent < 0 || parent >= static_cast<int>(m_nodes.size()))
		return;

	for (int child : m_nodes[parent].children)
	{
		if (m_nodes[child].name == name)
			out.push_back(child);
	}
}

const std::string& FbxAscii::Tree::Prop(int index, size_t slot) const
{
	if (index < 0 || index >= static_cast<int>(m_nodes.size()))
		return s_none;

	if (slot >= m_nodes[index].props.size())
		return s_none;

	return m_nodes[index].props[slot];
}

bool FbxAscii::Tree::Parse(const uint8_t* data, size_t size)
{
	m_nodes.clear();
	m_nodes.reserve(4096);
	Add(-1, "root");

	std::vector<int> stack;
	stack.push_back(0);

	int leaf = -1;
	size_t p = 0;
	std::string token;

	while (p < size)
	{
		size_t end = p;

		while (end < size && data[end] != '\n')
			++end;

		size_t begin = p;
		p = end + 1;

		while (begin < end && IsSpace(static_cast<char>(data[begin])))
			++begin;

		size_t stop = end;

		while (stop > begin && IsSpace(static_cast<char>(data[stop - 1])))
			--stop;

		if (begin >= stop || data[begin] == ';')
			continue;

		if (stop - begin == 1 && data[begin] == '}')
		{
			if (stack.size() > 1)
				stack.pop_back();

			leaf = -1;
			continue;
		}

		size_t colon = begin;

		while (colon < stop && IsNameByte(static_cast<char>(data[colon])))
			++colon;

		const bool header = colon > begin && colon < stop && data[colon] == ':';

		if (!header)
		{
			if (leaf < 0)
				continue;

			Node& node = m_nodes[leaf];
			token.clear();

			for (size_t i = begin; i <= stop; ++i)
			{
				const char c = i < stop ? static_cast<char>(data[i]) : ',';

				if (c != ',')
				{
					if (!IsSpace(c))
						token.push_back(c);

					continue;
				}

				double value = 0.0;

				if (Numeric(token, value))
					node.numbers.push_back(value);
				else if (!token.empty())
				{
					node.props.push_back(token);
					node.propAt.push_back(static_cast<uint32_t>(node.numbers.size()));
				}

				token.clear();
			}

			continue;
		}

		const std::string name(reinterpret_cast<const char*>(data + begin), colon - begin);

		size_t rest = colon + 1;

		while (rest < stop && IsSpace(static_cast<char>(data[rest])))
			++rest;

		bool opens = false;
		size_t tail = stop;

		if (tail > rest && data[tail - 1] == '{')
		{
			opens = true;
			--tail;

			while (tail > rest && IsSpace(static_cast<char>(data[tail - 1])))
				--tail;
		}

		const int index = Add(stack.back(), name);
		Node& node = m_nodes[index];

		token.clear();
		bool quoted = false;
		bool wasQuoted = false;

		for (size_t i = rest; i <= tail; ++i)
		{
			const char c = i < tail ? static_cast<char>(data[i]) : ',';

			if (c == '"')
			{
				quoted = !quoted;
				wasQuoted = true;
				continue;
			}

			if (c == ',' && !quoted)
			{
				double value = 0.0;

				if (!wasQuoted && Numeric(token, value))
				{
					node.numbers.push_back(value);
				}
				else if (!token.empty() || wasQuoted)
				{
					node.props.push_back(token);
					node.propAt.push_back(static_cast<uint32_t>(node.numbers.size()));
				}

				token.clear();
				wasQuoted = false;
				continue;
			}

			if (quoted || !IsSpace(c))
				token.push_back(c);
		}

		if (opens)
		{
			stack.push_back(index);
			leaf = -1;
			continue;
		}

		leaf = index;
	}

	return m_nodes.size() > 1;
}
