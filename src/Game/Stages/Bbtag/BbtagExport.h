#pragma once

#include "Game/Files/FbGameFolder.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace BbtagExport
{
	struct Framing
	{
		float scale[3];
		float position[3];
		float tilt;
		float turn;
	};

	struct Source
	{
		std::vector<uint8_t> model;
		std::function<bool(const std::string& name, std::vector<uint8_t>& out)> image;
		Framing framing;
		std::string stage;
	};

	struct File
	{
		std::string name;
		std::vector<uint8_t> data;
	};

	struct Result
	{
		std::string stage;
		std::vector<uint8_t> model;
		std::vector<uint8_t> bare;
		std::vector<File> motions;
		std::vector<File> scripts;
		std::vector<File> images;
		std::vector<std::string> missing;
		std::vector<std::string> foreign;
		int meshes = 0;
		int animated = 0;
		bool turned = false;
	};

	struct Archives
	{
		std::vector<uint8_t> scene;
		std::vector<uint8_t> geometry;
		std::vector<uint8_t> art;
	};

	Framing Neutral();

	void Placement(const Framing& framing, float out[16]);

	bool Convert(const Source& source, Result& out, std::string& error);

	bool Package(const Result& result, FbGameFolder::Game game, const std::string& model, Archives& out);
}
