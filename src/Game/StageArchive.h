#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace StageArchive
{
	struct Stage
	{
		std::string folder;
		std::string name;
		uint32_t bytes;
	};

	class Source
	{
	public:
		virtual ~Source() {}

		virtual void Stages(std::vector<Stage>& out) = 0;
		virtual void Files(const std::string& stage, std::vector<std::string>& out) = 0;
		virtual bool Read(const std::string& stage, const std::string& file,
			std::vector<uint8_t>& out) = 0;
		virtual bool BgList(std::string& out) = 0;
	};

	Source* Open(const char* folder);

	bool MagicOk(const std::string& file, const std::vector<uint8_t>& data);

	size_t MatchPair(const std::string& text, size_t open);

	bool Block(const std::string& bgList, const std::string& stage, std::string& out);
	bool Field(const std::string& block, const char* key, std::string& out);
}
