# Voices and sound

Swap a character's voice or a sound effect for another one. Only that character changes.

In the **Replace** tab, pick a character and press **Load voices and sounds**. You get everything
that character owns: battle voice, story lines, win quotes, announcer, menu and select lines, and the
shared effects it uses. Most battle lines show the spoken line beside them, read out of the game's
own sound list.

- **Play** — hear what plays now.
- **New...** — pick an .ogg, .wav or .mp3 from anywhere on your machine.
- **Back to original** — the game's sound again. Your file is kept, not deleted.

Your changes live in a pack. The first one asks for a name, makes it under `UNI2-IM\Sounds` and gives
it to that character. Everything after goes in the same one. The box beside the character switches
packs or goes back to the game's own sounds.

A sound is read when the character next loads, so leave the match or menu and come back to hear it.

## Taking a voice from UNI

**Get this voice from UNI...** takes a character's voice out of your own copy of the older game, the
folder with `UNIclr.exe`, `UNIst.exe` or `UNIEL.exe`. It brings the battle lines, win quotes and
story lines.

The two games do not name their files the same way, so the match is made on the line itself: both
write the spoken text beside every entry in their sound lists. About two thirds of a character's
lines find a partner. The rest keep their UNI2 recording, because UNI2 has lines the older game never
recorded. Tsurugi, Uzuki, Kaguya, Kuon, Ogre and Izumi are not in UNI at all.

It lands as a pack of its own. Running it again replaces what it added.

## Packs

A pack is a folder under `UNI2-IM\Sounds` holding files at the paths the game asks for. Which
character a file belongs to is read off its own path.

```
UNI2-IM/Sounds/my pack/pack.ini
UNI2-IM/Sounds/my pack/se/battle_se/chr000/hyd_010_b_2000.ogg     Hyde's voice
UNI2-IM/Sounds/my pack/se/normal_se/SE000.ogg                     a shared sound, for everybody
UNI2-IM/Sounds/my pack/chr000/SE_InsulatorSwingA.ogg              Hyde's own copy of a shared sound
```

That last line is the one people ask for. Some effects are shared across the cast, and a `chrNNN`
folder at the top of a pack holds one character's private copies. The mod rewrites that character's
sound list to reach for them and leaves the other twenty-six on the original.

`pack.ini` is optional. `Character` says who owns the files whose path does not name anyone, like the
story lines under `se\talk`.

```ini
[Pack]
Name      = Hyde, UNI cl-r
Author    = you
Source    = UNDER NIGHT IN-BIRTH Exe:Late[cl-r]
Character = 0
```

**Use Ogg Vorbis.** WAV and MP3 are converted for you once and cached. They cannot be handed to the
game as they are: the engine only reads a WAV out of its own archive, and a loose one comes out
silent. The name is what the game asks for; the extension does not have to match, because the engine
reads the first bytes of the file, not its name.

**Export** writes a zip you can send to somebody. **Import** puts one back. Nothing ships with the
mod. The audio is yours, from a game you own.

`UNI2-IM\Sounds\README.txt` says the same thing outside the game.
