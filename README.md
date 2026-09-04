# UNI2 Improvement Mod

Training and quality-of-life mod for **UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`).

It loads as a `dinput8.dll` proxy and draws a Dear ImGui overlay inside the game's Direct3D 9
renderer. Built on the architecture of
[BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod).

**[Download](https://github.com/Zanaylo/UNI2-Improvement-Mod/releases)** ·
**[Wiki](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki)** ·
[Português](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Portugues) ·
[日本語](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Japanese)

## Install

1. Download the zip from the [releases page](https://github.com/Zanaylo/UNI2-Improvement-Mod/releases).
2. In Steam: right click the game → **Manage** → **Browse local files**.
3. Extract the zip there, next to `uni2.exe`. You get `dinput8.dll` and `UNI2IMUpdater.exe`.
4. Start the game and press **F1**.

`UNI2IMUpdater.exe` installs later versions. It does nothing on its own.

To uninstall, delete both files. Nothing else is touched.

**Linux and Steam Deck.** Same two files plus one launch option. Right click the game →
**Properties** → **Launch options**, and paste this exactly:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

Nothing gets renamed. Without that line Wine never loads the DLL, and the mod does nothing.

On Windows, RivaTuner and MSI Afterburner have to be told to leave `uni2.exe` alone. That and the
rest is in [Installing](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Installing).

## What it does

**Training** — [hitbox viewer](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Hitbox-viewer),
[frame meter](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Frame-meter),
[pause and frame stepping](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Pause-and-frame-stepping),
[Player Control](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Player-Control),
[keyboard side](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Keyboard-side).

**Yours** — [palettes](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Palettes),
[voices and sound](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Voices-and-sound),
[BGM](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/BGM-selector),
[player card](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Player-Card),
[shaders](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Shaders),
[stages](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Stages).

**The game itself** — [performance](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Performance),
[POTATO MODE](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/POTATO-MODE),
[improvements](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Improvements).

Every setting lives in the ini beside the DLL. All of them are listed on
[The ini file](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/The-ini-file).

## Online play

**Nothing here is built to give an advantage online, and nothing here does.**

Every tool that can change the simulation is locked to offline modes. If the game has sent a packet
to an opponent in the last three seconds, those tools refuse to run. What still runs online is
cosmetic and read-only.

**The patch selector is the exception, and it is offline only.** A patch loads at startup and stays
for the whole session, so it is still live online. It only works against someone on the same patch.
**Do not use it in ranked, or against anyone who is not on it. It will desync.** Restart on the
installed game first.

If you find something here that gives an edge in a real match, that is a bug. Report it.
[More](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Online-play).

## Credits

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) - architecture reference
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) - HA6 / CG / PAL format ground truth
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) - modding documentation
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

Thanks to Pescador Cearense, Eon, Listentothebirds - Rafael, Willyofruit, Sky Leite, Excel, ZateFGC,
Yorezordd (Velho fudido), Thiago, Tanasinn [AZ], Licensed Grappler and Anklegator.

Building it yourself: [Building](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Building).
