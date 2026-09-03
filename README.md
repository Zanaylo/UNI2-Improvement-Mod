# UNI2 Improvement Mod

Training and quality-of-life mod for **UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`).

It loads as a `dinput8.dll` proxy and draws a Dear ImGui overlay inside the game's Direct3D 9
renderer. Inspired by, and architecturally indebted to,
[BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod).

**[Download](https://github.com/Zanaylo/UNI2-Improvement-Mod/releases)** ·
**[Wiki](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki)** ·
[Português](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Portugues) ·
[日本語](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Japanese)

## Install

1. Download `dinput8.dll` from the [releases page](https://github.com/Zanaylo/UNI2-Improvement-Mod/releases).
2. In Steam, right click the game → **Manage** → **Browse local files**.
3. Drop `dinput8.dll` in that folder, next to `uni2.exe`.
4. Start the game and press **F1**.

To uninstall, delete `dinput8.dll`. Nothing else is touched.

On Linux or Steam Deck the file has to be named `d3d9.dll` instead, and Proton needs one launch
option. On Windows, RivaTuner and MSI Afterburner have to be told to leave `uni2.exe` alone. Both
are in [Installing](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Installing).

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
[extra stages](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Extra-stages).

**The game itself** — [performance](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Performance),
[POTATO MODE](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/POTATO-MODE),
[improvements](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/Improvements).

Every setting is in the ini beside the DLL and is documented on
[The ini file](https://github.com/Zanaylo/UNI2-Improvement-Mod/wiki/The-ini-file).

## Online play

**Nothing in this mod is built to give anyone an advantage online, and nothing in it does.**

Every tool that can alter the simulation is hard-gated to offline modes: while the game has sent a
packet to an opponent in the last three seconds, they refuse to run. What does run online is
cosmetic and read-only.

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
