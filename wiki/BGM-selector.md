# BGM selector

Open **Music** in the mod menu. It has its own window.

You can give any screen with music a different track: a character's battle theme, character select,
the VS screen, a menu. The game ships three matchup themes. This does the same thing, with no limit.

## Soundpacks

A soundpack installs a whole game's soundtrack at once and sets every screen to use it.

**Get OST from French-Bread games** takes the soundtrack from a copy you already own. Point it at the
folder with `UNIclr.exe` or `UNIst.exe`, `MBTL.exe`, or `MBAA.exe`. It installs the tracks with song
titles and loop points. Nothing is downloaded and no audio ships with the mod. If you run it twice on
the same game, it replaces what it added instead of doubling it.

**Export** and **Import** save your packs to a single zip and load them back, so a friend can get the
same set in one step.

## Browse

Every track the game can play. Search it and filter it by source. **Play** starts a track and keeps
it playing. **Stop** gives the game its music back.

The **Randomizer** gives the game a random track from this list every time it asks for music. Each
track has a switch, and a track you turn off is never picked.

Each track has its own **Volume**. It is saved in `UNI2-IM/bgm.ini` and applied when you release the
slider. It can only make a track quieter: 100% is the level the track was recorded at, and you cannot
go higher. So if one track is too quiet, raise the game's BGM volume until that track sounds right,
then lower the ones that are now too loud here (usually the battle themes). **Reset volumes** sets
everything back to 100%.

## Rules

Set music by hand: play this track for this matchup, for this character, or instead of this screen.
Rules can be exported and imported too. Importing adds to your list instead of replacing it.

## Add music

**Import music** takes an MP3, OGG or WAV. You can also drop files into `UNI2-IM/Music` yourself,
loose or in one folder per pack. They show up in Browse with everything else.

The game only opens a loose file if it is OGG Vorbis, so MP3 and WAV files are converted when you add
them. The tab lists every file it found and says why a file was skipped. Usually it is an `.ogg` that
holds Opus instead of Vorbis. Long names are fine: a name longer than the game's 31-character limit no
longer loses the track.

Each track has a **Loop from** point in seconds, like a soundpack track. Leave it at 0 and the whole
song repeats, intro included. Set it after the intro and the track loops like the game's own music.

**New soundpack**, at the top of Browse, builds a pack of your own. Tick the tracks you want in the
**Pack** column, choose the screen each one plays on, and save. The pack sits next to the packs that
ship with the mod and is included when you Export.
