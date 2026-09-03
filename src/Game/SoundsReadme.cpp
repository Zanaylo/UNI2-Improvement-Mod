#include "Game/SoundsReadme.h"

#include "Core/FileIndex.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kFileName = "README.txt";

constexpr const char* kLines[] = {
"VOICES AND SOUNDS",
"=================",
"",
"Everything in this folder replaces a sound the game would otherwise play. Nothing here",
"touches the game's own files, so deleting a folder puts the game back the way it was.",
"",
"",
"THE EASY WAY - the Replace tab",
"------------------------------",
"",
"Open the mod overlay, go to Voices and sound, and use the Replace tab.",
"",
"  1. Pick a character, and beside it the pack that character wears.",
"  2. Press \"Load voices and sounds\". Every sound that character owns is listed - the",
"     battle voice, the win quotes, the character select lines, the story lines, and the",
"     sound effects that character asks for.",
"  3. Play - hear what is playing right now.",
"     New  - choose a file of your own from anywhere on your computer.",
"     Back to original - drop that one change and let the game's own sound play again.",
"",
"Your changes have to live in a pack, so the first New on a character still using the",
"game's own sounds asks you to name one. It is made under Sounds\\ and worn by that",
"character straight away; everything you change afterwards goes into the same pack. Give",
"it to a friend from the Packs tab, or pick a different one at any time and the first is",
"left exactly as it was.",
"",
"\"Back to original\" does not delete anything. The file moves into the pack's own .off",
"folder, out of the game's way, and pressing New again overwrites it.",
"",
"",
"A WHOLE VOICE FROM UNI",
"----------------------",
"",
"The same tab has \"Get this voice from UNI...\". Point it at the folder holding UNIclr.exe,",
"UNIst.exe or UNIEL.exe - your own copy of the older game - and the mod lifts that character's",
"voice out of it: the battle lines, the win quotes and the story lines.",
"",
"It knows which file answers which because both games write the spoken line beside every entry",
"in their own sound list, so the line is matched on its text and, failing that, on its number.",
"Around two thirds of a character's lines find a partner; the rest keep their UNI2 recording,",
"because UNI2 simply has lines the older game never recorded.",
"",
"What it takes lands as its own pack - Sounds\\<character> - UNI cl-r - worn by that one",
"character. The pack box beside the character switches back to the game's own voice at any",
"time, nothing is deleted, and running the import again replaces what it added rather than",
"doubling it.",
"",
"The files arrive as WAV and are converted to Ogg once, which takes a few seconds and is what",
"the Packs tab is counting when it says \"converting\".",
"",
"",
"WHEN A CHANGE IS HEARD",
"----------------------",
"",
"A sound is read when the character loads it, so a change lands the next time you enter a",
"match, a menu, or the gallery. If in doubt, back out to the main menu and go in again.",
"",
"",
"PACKS - a whole voice in one folder",
"-----------------------------------",
"",
"A pack is a folder in here whose files sit at the same paths the game asks for. Which",
"character a file belongs to is read off its own path, so there is nothing to declare:",
"",
"  Sounds\\my pack\\pack.ini",
"  Sounds\\my pack\\se\\battle_se\\chr000\\hyd_010_b_2000.ogg      Hyde's battle voice",
"  Sounds\\my pack\\se\\winner_message\\chr000\\chr000_00_1000.ogg  Hyde's win quote",
"  Sounds\\my pack\\se\\normal_se\\SE000.ogg                       a sound everybody shares",
"  Sounds\\my pack\\chr000\\SE_InsulatorSwingA.ogg                 Hyde's own copy of a",
"                                                               shared sound",
"",
"pack.ini is three lines and all of them are optional:",
"",
"  [Pack]",
"  Name      = Hyde, UNI cl-r",
"  Author    = you",
"  Source    = UNDER NIGHT IN-BIRTH Exe:Late[cl-r]",
"  Character = 0",
"",
"Character is the one extra: 0 is Hyde, and it says who owns the files in the pack whose path",
"does not name a character - the story lines under se\\talk, for instance. Leave it out and",
"those files count as everybody's.",
"",
"The box beside the character in the Replace tab is what gives a pack to that character,",
"and it is the only place that choice is made. A pack carrying sounds that belong to",
"nobody in particular - se\\normal_se and the flat se\\battle_se - offers a tick in the",
"Packs tab to let everybody hear them.",
"",
"A chrNNN folder at the top of a pack holds that one character's private copies of sounds",
"the whole roster shares, named after the sound they stand in for. The mod rewrites that",
"character's own sound list to reach for them, so the other characters keep the original.",
"That is the same thing the Replace tab does for you when you replace a Shared sound.",
"",
"",
"FILE FORMATS",
"------------",
"",
"Ogg Vorbis plays as it is. WAV and MP3 are converted to Ogg once and remembered, because",
"the engine only reads a WAV out of its own archive - a loose one is silent. The name you",
"give a file is the name the game asks for; the extension does not have to match, since",
"the engine reads the first bytes rather than the name.",
"",
"Mono or stereo, 8 to 192 kHz.",
"",
"",
"SENDING ONE TO A FRIEND",
"-----------------------",
"",
"The Packs tab has Export, which writes the whole folder into a zip, and Import, which",
"unpacks somebody else's zip back in here. The pack you made yourself in the Replace tab",
"goes the same way - it is a pack like any other.",
"",
};

std::string Text()
{
	std::string out;

	for (const char* line : kLines)
	{
		out += line;
		out += "\r\n";
	}

	return out;
}

bool Matches(const std::string& path, const std::string& text)
{
	std::vector<uint8_t> current;

	if (!ReadWholeFile(path, current))
		return false;

	if (current.size() != text.size())
		return false;

	return memcmp(current.data(), text.c_str(), text.size()) == 0;
}

}

void SoundsReadme::Write(const std::string& folder)
{
	const std::string path = FileIndex::Join(folder, kFileName);
	const std::string text = Text();

	if (Matches(path, text))
		return;

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
		return;

	fwrite(text.c_str(), 1, text.size(), handle);
	fclose(handle);
}

std::string SoundsReadme::Path()
{
	return FileIndex::Join(GetModRootPath("Sounds"), kFileName);
}
