# BGM selector

Its own window, opened from **Music** in the mod menu.

Every screen with music goes through one entry point in the game, so any of them can be given a
different track: a character's battle theme, character select, the VS screen, a menu. The game ships
three matchup themes. This is the same idea without the limit.

## Soundpacks

Installs a whole game's soundtrack at once and points every screen at it.

**Get OST from French-Bread games** reads one out of a copy you already own. Point it at the folder
holding `UNIclr.exe` or `UNIst.exe`, `MBTL.exe`, or `MBAA.exe`. It installs the tracks with song
titles and loop points. Nothing is downloaded and no audio ships with the mod. Running it twice on
the same game replaces what it added instead of doubling it.

**Export** and **Import** move your packs in and out of a single zip, so a friend can have the same
set without repeating any of this.

## Browse

Every track the game can play, searchable and filtered by source. **Play** starts one and holds it;
the game gets its music back on **Stop**.

The **Randomizer** hands the game a random track from that list every time it asks for music. Every
track has a switch, so anything you turn off is never picked.

Every track has a **Volume** of its own, kept in `UNI2-IM/bgm.ini` and applied when you let go of the
slider. It is the engine's own per-slot field and it can only hold a track back: 100% is the level
the track was recorded at, and there is nothing past it. So to hear one quiet track properly, raise
the game's BGM volume until that one is right, then pull the ones that are now too loud, usually the
battle themes, down here. **Reset volumes** puts everything back to 100%.

## Rules

The manual version: play this track for this matchup, for this character, or in place of this screen.
Rules export and import too, and importing adds to your list instead of replacing it.

## Add music

**Import music** takes an MP3, OGG or WAV. Or drop files into `UNI2-IM/Music` yourself, loose or a
folder per pack, and they show up in Browse with everything else.

The game only opens a loose file if it is OGG Vorbis, so an MP3 or WAV is re-encoded on the way in.
The tab lists every file it found and says why anything was skipped. Usually that is an `.ogg` that
turned out to hold Opus, not Vorbis. A name too long for the game's 31-character slot field no longer
costs you the track.

Each track gets a **Loop from** point in seconds, the same thing a soundpack track carries. Leave it
at 0 and the whole song repeats, intro and all. Set it past the intro and it loops the way the game's
own music does.

**New soundpack**, at the top of Browse, builds one of your own: tick the tracks you want in the
**Pack** column, choose which screen each plays on, and save. It lands beside the packs that ship
with the mod and travels with Export.
