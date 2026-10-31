#include "Core/Config/FeatureCatalog.h"

namespace {

const FeatureCatalog::Entry kEntries[FeatureCatalog::Id_COUNT] = {
	{ "Music", "Music", "Soundpacks, your tracks and the rules for what plays where." },
	{ "Subtitles", "Subtitles", "On-screen lines for what each character says." },
	{ "Privacy", "Privacy", "Hides opponent names, room names and IR." },
	{ "NetcodePatches", "Netcode patches", "The mod's time sync and round trip tuning for online matches." },
	{ "NetplayWindow", "Netplay window", "Rollback, ping, the room and your match history." },
	{ "Training", "Training", "Hitboxes, the frame meter, frame stepping, the dummy and the training HUD." },
	{ "Palettes", "Palettes", "Custom palettes, the colour editor and sharing palettes online." },
	{ "Stages", "Stages", "Added and hidden stages, stage cards and the random stage pick." },
	{ "Replays", "Replays", "Replay files, Start Replay and keeping the replay when the game closes." },
	{ "VoicesAndSound", "Voices and sound", "Voice swaps and sound effect packs." },
	{ "GamePatches", "Game patches", "Playing an older version's battle data. Online, the host's version still applies." },
	{ "CharacterArt", "Character art and announcers", "Alternate character art and the MELTY BLOOD TYPE LUMINA announcers." },
	{ "ModPacks", "Mod packs", "Folders in UNI2-IM\\Packs that replace game files." },
	{ "Graphics", "Graphics", "Output and stage resolution, shaders, POTATO MODE and frame pacing." },
};

const FeatureCatalog::Entry kUnknown = { "Unknown", "Unknown", "Not a module." };

bool InRange(int id)
{
	return id >= 0 && id < FeatureCatalog::Id_COUNT;
}

}

const FeatureCatalog::Entry& FeatureCatalog::Get(Id id)
{
	if (!InRange(id))
		return kUnknown;

	return kEntries[id];
}
