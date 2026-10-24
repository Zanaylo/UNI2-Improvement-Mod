#pragma once

namespace GraphicsOff
{
	constexpr int kGameSize = 0;
	constexpr int kFullScene = 100;

	template <typename Values>
	void Apply(Values& values)
	{
		values.supersample = kGameSize;
		values.presentWidth = kGameSize;
		values.presentHeight = kGameSize;
		values.internalResolution = kGameSize;
		values.potatoStage = kGameSize;
		values.potatoMode = 0;
		values.upscaleFilter = 0;
		values.antiAliasing = 0;
		values.sharpenMode = 0;
		values.bloomEnabled = false;
		values.lookEnabled = false;
		values.sceneScalePercent = kFullScene;
		values.disableBackBufferAa = false;
		values.disableCharacterFilter = false;
		values.simpleStage = false;
		values.displaySync = false;
	}
}
