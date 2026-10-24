#pragma once

namespace FeatureCatalog
{
	enum Id
	{
		Id_Music,
		Id_Subtitles,
		Id_Privacy,
		Id_NetcodePatches,
		Id_NetplayWindow,
		Id_Training,
		Id_Palettes,
		Id_Stages,
		Id_Replays,
		Id_VoicesAndSound,
		Id_GamePatches,
		Id_CharacterArt,
		Id_ModPacks,
		Id_Graphics,
		Id_COUNT
	};

	struct Entry
	{
		const char* key;
		const char* name;
		const char* summary;
	};

	const Entry& Get(Id id);
}
