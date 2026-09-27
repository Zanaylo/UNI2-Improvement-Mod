#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BbtagParticle
{
	struct Group
	{
		uint32_t flags;
		int countMin;
		int countMax;
		int lifeMin;
		int lifeMax;
		int delayMin;
		int delayMax;
	};

	struct Range
	{
		double min;
		double max;
	};

	struct Size
	{
		Range width;
		Range height;
	};

	struct Sprite
	{
		uint32_t flags;
		uint32_t flags2;
		Range rotationStart;
		Range rotationSpeed;
		Range rotationAccel;
		double rotationDamping;
		int colourPeriodMin;
		int colourPeriodMax;
		uint32_t colourStart;
		uint32_t colourMid;
		uint32_t colourEnd;
		double colourMidAt;
		int scalePeriodMin;
		int scalePeriodMax;
		Size scaleStart;
		Size scaleMid;
		Size scaleEnd;
		double scaleMidAt;
		int uvanime;
		double uv[4];
		int sharedAtlas;
	};

	struct Shape
	{
		uint32_t flags;
		double radius;
		double angleRange;
		double angleOffset;
		double boxMin[3];
		double boxMax[3];
		double sizeScale;
	};

	struct Move
	{
		uint32_t flags;
		Range velocity[3];
		Range accel[3];
		double damping;
	};

	struct Effect
	{
		std::string name;
		Group group;
		Sprite sprite;
		Shape shape;
		Move move;
	};

	struct Surface
	{
		int width;
		int height;
		std::vector<uint8_t> bgra;
	};

	bool Read(const std::vector<uint8_t>& pac, std::vector<Effect>& out);

	bool Atlas(const std::vector<uint8_t>& pac, Surface& out);

	const Effect* Find(const std::vector<Effect>& effects, const std::string& name);
}
