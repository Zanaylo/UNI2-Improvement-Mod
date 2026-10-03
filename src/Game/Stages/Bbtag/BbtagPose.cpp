#include "Game/Stages/Bbtag/BbtagPose.h"

#include <cmath>
#include <cstring>

namespace {

constexpr float kSingular = 1e-12f;

void Rotation(const float rows[3][3], float out[4])
{
	const float trace = rows[0][0] + rows[1][1] + rows[2][2];

	if (trace > 0.0f)
	{
		const float s = sqrtf(trace + 1.0f) * 2.0f;
		out[3] = 0.25f * s;
		out[0] = (rows[1][2] - rows[2][1]) / s;
		out[1] = (rows[2][0] - rows[0][2]) / s;
		out[2] = (rows[0][1] - rows[1][0]) / s;
		return;
	}

	if (rows[0][0] > rows[1][1] && rows[0][0] > rows[2][2])
	{
		const float s = sqrtf(1.0f + rows[0][0] - rows[1][1] - rows[2][2]) * 2.0f;
		out[3] = (rows[1][2] - rows[2][1]) / s;
		out[0] = 0.25f * s;
		out[1] = (rows[1][0] + rows[0][1]) / s;
		out[2] = (rows[2][0] + rows[0][2]) / s;
		return;
	}

	if (rows[1][1] > rows[2][2])
	{
		const float s = sqrtf(1.0f + rows[1][1] - rows[0][0] - rows[2][2]) * 2.0f;
		out[3] = (rows[2][0] - rows[0][2]) / s;
		out[0] = (rows[1][0] + rows[0][1]) / s;
		out[1] = 0.25f * s;
		out[2] = (rows[2][1] + rows[1][2]) / s;
		return;
	}

	const float s = sqrtf(1.0f + rows[2][2] - rows[0][0] - rows[1][1]) * 2.0f;
	out[3] = (rows[0][1] - rows[1][0]) / s;
	out[0] = (rows[2][0] + rows[0][2]) / s;
	out[1] = (rows[2][1] + rows[1][2]) / s;
	out[2] = 0.25f * s;
}

void Angles(const float rows[3][3], float out[3])
{
	const float sine = rows[1][2] > 1.0f ? 1.0f : (rows[1][2] < -1.0f ? -1.0f : rows[1][2]);

	out[0] = asinf(sine);
	out[1] = atan2f(-rows[0][2], rows[2][2]);
	out[2] = atan2f(-rows[1][0], rows[1][1]);
}

void Turn(const float angles[3], float out[4])
{
	const float hx = angles[0] * 0.5f;
	const float hy = angles[1] * 0.5f;
	const float hz = angles[2] * 0.5f;

	const float x[4] = { sinf(hx), 0.0f, 0.0f, cosf(hx) };
	const float y[4] = { 0.0f, sinf(hy), 0.0f, cosf(hy) };
	const float z[4] = { 0.0f, 0.0f, sinf(hz), cosf(hz) };

	auto product = [](const float a[4], const float b[4], float result[4])
	{
		result[0] = a[3] * b[0] + a[0] * b[3] + a[2] * b[1] - a[1] * b[2];
		result[1] = a[3] * b[1] + a[1] * b[3] + a[0] * b[2] - a[2] * b[0];
		result[2] = a[3] * b[2] + a[2] * b[3] + a[1] * b[0] - a[0] * b[1];
		result[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
	};

	float yx[4] = {};
	product(y, x, yx);
	product(yx, z, out);
}

}

bool BbtagPose::Split(const float matrix[16], Pose& out)
{
	out = Pose();

	float rows[3][3] = {};

	for (int r = 0; r < 3; ++r)
	{
		float length = 0.0f;

		for (int c = 0; c < 3; ++c)
			length += matrix[r * 4 + c] * matrix[r * 4 + c];

		length = sqrtf(length);

		if (length < kSingular)
			return false;

		out.scale[r] = length;

		for (int c = 0; c < 3; ++c)
			rows[r][c] = matrix[r * 4 + c] / length;
	}

	const float determinant = rows[0][0] * (rows[1][1] * rows[2][2] - rows[1][2] * rows[2][1])
		- rows[0][1] * (rows[1][0] * rows[2][2] - rows[1][2] * rows[2][0])
		+ rows[0][2] * (rows[1][0] * rows[2][1] - rows[1][1] * rows[2][0]);

	if (determinant < 0.0f)
	{
		out.scale[0] = -out.scale[0];

		for (int c = 0; c < 3; ++c)
			rows[0][c] = -rows[0][c];
	}

	for (int k = 0; k < 3; ++k)
		out.translation[k] = matrix[12 + k];

	Rotation(rows, out.turn);
	Angles(rows, out.rotation);

	return true;
}

void BbtagPose::Compose(const Pose& pose, float out[16])
{
	const float x = pose.turn[0];
	const float y = pose.turn[1];
	const float z = pose.turn[2];
	const float w = pose.turn[3];

	const float rows[3][3] = {
		{ 1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y + z * w), 2.0f * (x * z - y * w) },
		{ 2.0f * (x * y - z * w), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z + x * w) },
		{ 2.0f * (x * z + y * w), 2.0f * (y * z - x * w), 1.0f - 2.0f * (x * x + y * y) },
	};

	memset(out, 0, sizeof(float) * 16);

	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
			out[r * 4 + c] = pose.scale[r] * rows[r][c];
	}

	for (int k = 0; k < 3; ++k)
		out[12 + k] = pose.translation[k];

	out[15] = 1.0f;
}

void BbtagPose::Turned(Pose& pose)
{
	Turn(pose.rotation, pose.turn);
}
