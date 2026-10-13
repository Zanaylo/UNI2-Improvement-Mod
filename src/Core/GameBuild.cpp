#include "Core/GameBuild.h"

#include "Core/info.h"

GameBuild::Age GameBuild::Classify(uint32_t stamp, uint32_t imageSize)
{
	if (stamp < UNI2_IM_SUPPORTED_GAME_STAMP)
		return Age::Older;

	if (stamp == UNI2_IM_SUPPORTED_GAME_STAMP && imageSize == UNI2_IM_SUPPORTED_GAME_IMAGE_SIZE)
		return Age::Supported;

	return Age::Newer;
}
