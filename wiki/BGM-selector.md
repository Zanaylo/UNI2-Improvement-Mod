# BGM selector

Its own window, opened from **Music** in the mod menu.

Every screen with music goes through one entry point in the game, so any of them can be given a
different track: a character's battle theme, the character select, the VS screen, a menu. The game
ships three matchup themes; this is the same idea without the limit.

**Soundpacks** installs a whole game's soundtrack at once and points every screen at it. **Get OST
from French-Bread games** reads one out of a copy you already own - point it at the folder holding
`UNIclr.exe` or `UNIst.exe`, `MBTL.exe`, or `MBAA.exe` - and installs it here with song titles and
loop points. Nothing is downloaded, and no audio ships with the mod. Running it twice on the same
game replaces what it added rather than doubling it. Export and Import move your packs to and from
a single zip, so a friend can have the same set without repeating any of this.

**Browse** lists every track the game can play, searchable and filtered by source, with Play to
start one and hold it - the game gets its music back when you press Stop. The **Randomizer** hands
the game a random track from that list every time it asks for music, and every track has a switch,
so anything you turn off is never picked.

Every track also has a **Volume** of its own, remembered in `UNI2-IM/bgm.ini` and applied the moment
you let go of the slider. It is the engine's own per-slot field, which can only hold a track back:
100% is the level the track was recorded at and there is no way past it. So the way to hear one
quiet track properly is to raise the game's BGM volume until that one is right, then pull the ones
that are now too loud - the battle themes, usually - down here. **Reset volumes** puts every track
back to 100%.

**Rules** is the manual version: play this track for this matchup, for this character, or in place
of this screen. Rules can be exported and imported as well, and importing adds to your list instead
of replacing it.

**Add music** takes your own. Press **Import music** and pick an MP3, OGG or WAV, or drop files into
`UNI2-IM/Music` yourself - loose or a folder per pack - and they turn up in Browse beside everything
else. The game will open a loose file only when it is OGG Vorbis, so an MP3 or a WAV is re-encoded
on the way in. The tab lists every file it found and says plainly why anything was skipped, which is
usually an `.ogg` that turned out to hold Opus rather than Vorbis. A name too long for the game's 31
character slot field no longer costs you the track.

Each of your tracks gets a **Loop from** point there too, in seconds, the same thing every soundpack
track carries. Leave it at 0 and the whole song repeats, intro and all; set it past the intro and it
loops the way the game's own music does.

**New soundpack**, at the top of Browse, builds one of your own: tick the tracks you want in the
**Pack** column, choose which screen each one plays on, and save - it lands beside the packs that
ship with the mod and travels with Export.
