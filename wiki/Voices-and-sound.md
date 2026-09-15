# Voices and sound

Swap a character's voice or a sound effect for another one. Only that character changes.

In the **Replace** tab, pick a character and press **Load voices and sounds**. You get everything that
character owns: battle voice, story lines, win quotes, announcer, menu and select lines, and the
shared effects it uses. Most battle lines show the spoken text next to them, taken from the game's own
sound list.

- **Play**: hear what plays now.
- **New...**: pick an .ogg, .wav or .mp3 from anywhere on your computer.
- **Back to original**: the game's sound again. Your file is kept, not deleted.

Your changes are saved in a pack. The first change asks for a name, creates the pack under
`UNI2-IM\Sounds` and gives it to that character. Everything after that goes in the same pack. The box
next to the character switches packs or goes back to the game's own sounds.

A sound is read when the character loads next, so leave the match or menu and come back to hear it.

## Taking a voice from UNI

**Get this voice from UNI...** takes a character's voice from your own copy of the older game (the
folder with `UNIclr.exe`, `UNIst.exe` or `UNIEL.exe`). It brings the battle lines, win quotes and
story lines.

The two games name their files differently, so the mod matches them by the line itself: both games
write the spoken text next to every entry in their sound lists. About two thirds of a character's
lines find a match. The rest keep their UNI2 recording, because UNI2 has lines the older game never
recorded. Tsurugi, Uzuki, Kaguya, Kuon, Ogre and Izumi are not in UNI at all.

The result is a pack of its own. Running it again replaces what it added.

## Packs

A pack is a folder under `UNI2-IM\Sounds` with files at the paths the game asks for. The path of each
file tells the mod which character it belongs to.

```
UNI2-IM/Sounds/my pack/pack.ini
UNI2-IM/Sounds/my pack/se/battle_se/chr000/hyd_010_b_2000.ogg     Hyde's voice
UNI2-IM/Sounds/my pack/se/normal_se/SE000.ogg                     a shared sound, for everybody
UNI2-IM/Sounds/my pack/chr000/SE_InsulatorSwingA.ogg              Hyde's own copy of a shared sound
```

That last line is the one people ask about. Some effects are shared by the whole cast. A `chrNNN`
folder at the top of a pack holds private copies for one character. The mod changes that character's
sound list to use them, and the other twenty-six keep the original.

`pack.ini` is optional. `Character` says who owns the files whose path does not name a character, like
the story lines under `se\talk`.

```ini
[Pack]
Name      = Hyde, UNI cl-r
Author    = you
Source    = UNDER NIGHT IN-BIRTH Exe:Late[cl-r]
Character = 0
```

**Use Ogg Vorbis.** WAV and MP3 are converted for you once and cached. The game cannot use them
directly: the engine only reads a WAV from its own archive, and a loose WAV plays silent. The file name
must be the one the game asks for, but the extension does not have to match, because the engine reads
the first bytes of the file, not its name.

**Export** writes a zip you can send to someone. **Import** loads one back. No audio ships with the
mod. The audio is yours, from a game you own.

`UNI2-IM\Sounds\README.txt` explains the same thing outside the game.
