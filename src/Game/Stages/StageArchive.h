#pragma once

#include "Game/Stages/Bbtag/BbtagScript.h"

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

	struct Pair
	{
		std::string key;
		std::string value;
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

		virtual bool Flow(const std::string&, std::vector<float>&) { return false; }
		virtual bool Lamps(const std::string&, std::vector<BbtagScript::Lamp>&)
		{
			return false;
		}
		virtual bool Fading(const std::string&) { return false; }
	};

	Source* Open(const char* folder);

	bool Asset(const char* folder, const char* path, std::vector<uint8_t>& out);

	bool MagicOk(const std::string& file, const std::vector<uint8_t>& data);

	size_t MatchPair(const std::string& text, size_t open);

	bool Block(const std::string& bgList, const std::string& stage, std::string& out);
	bool FieldSpan(const std::string& block, const char* key, size_t& valueAt, size_t& valueEnd);
	bool Field(const std::string& block, const char* key, std::string& out);
	void Pairs(const std::string& block, std::vector<Pair>& out);
	std::string Unquoted(const std::string& value);

	int CardIndex(const std::string& bgList, const std::string& stage);
}
